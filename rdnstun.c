#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include "macro.h"
#include "utils.h"
#include "log.h"
#include "iface.h"
#include "chain.h"
#include "rdnstun.h"


volatile bool rdnstun_shutdown = false;


static void shutdown_rdnstun (int sig) {
  (void) sig;
  LOGGER(RDNSTUN_NAME, LOG_LEVEL_INFO, "Shutting down " RDNSTUN_NAME);
  rdnstun_shutdown = true;
}


static int cread (int fd, void *buf, size_t count){
  int nread = read(fd, buf, count);
  should (nread > 0) otherwise {
    perror("read");
  }
  return nread;
}


static int cwrite (int fd, const void *buf, size_t count){
  int nwrite = write(fd, buf, count);
  should (nwrite >= 0) otherwise {
    perror("write");
  }
  return nwrite;
}


static void rdnstun (
    int tunfd, const struct HostChain *v4_chains,
    const struct HostChain *v6_chains, volatile bool *shutdown) {
  struct pollfd fds[] = {
    {.fd = tunfd, .events = POLLIN},
  };

  for (int pollres; !*shutdown;) {
    pollres = poll(fds, arraysize(fds), RDNSTUN_SLEEP_TIME * 1000);
    should (pollres >= 0) otherwise {
      if (*shutdown) {
        break;
      }
      perror("poll");
      continue;
    }
    if (pollres == 0) {
      continue;
    }

    logger_end(LOG_LEVEL_DEBUG);
    // data from tun/tap: read it
    unsigned char packet[IP_MAXPACKET];
    int pkt_receive_len = cread(tunfd, packet, sizeof(packet));
    break_if_fail (pkt_receive_len > 0);
    LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
           "Read %d bytes from the interface", pkt_receive_len);

    unsigned short pkt_send_len = pkt_receive_len;
    unsigned char ipver = ((struct ip *) packet)->ip_v;
    switch (ipver) {
      int ret;
      case 4:
        goto_if_fail (v4_chains != NULL) undefined_ipver;
        goto_nonzero (HostChain4Array_reply(
          v4_chains, packet, &pkt_send_len)) fail_reply;
        break;
      case 6:
        goto_if_fail (v6_chains != NULL) undefined_ipver;
        goto_nonzero (HostChain6Array_reply(
          v6_chains, packet, &pkt_send_len)) fail_reply;
        break;
      default:
        LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "Unknown IP version %d", ipver);
        if (0) {
undefined_ipver:
          LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
                 "Received IPv%d packet but no IPv%d chains defined",
                 ipver, ipver);
        }
        if (0) {
fail_reply:
          switch (ret) {
            case 17:
              LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "No host to reply");
              break;
            case 18:
              LOGGER(RDNSTUN_NAME, LOG_LEVEL_WARNING,
                     "Received packet with TTL 0");
              break;
            case 19:
              LOGGER(RDNSTUN_NAME, LOG_LEVEL_WARNING,
                     "Host TTL too small, this is a bug");
              break;
            default:
              LOGGER(RDNSTUN_NAME, LOG_LEVEL_WARNING, "Unknown error number %d",
                     ret);
          }
        }
        continue;
    }
    // write it into the tun/tap interface
    if likely (pkt_send_len > 0) {
      int n_write = cwrite(tunfd, packet, pkt_send_len);
      if likely (n_write >= 0) {
        LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
               "Write %d bytes to the interface", n_write);
      }
    }
  }
}


static void usage (const char *progname) {
  fprintf(stderr, "Usage: %s [OPTIONS]... [<iface>]\n", progname);
  fputs(
"\n"
"Address chain may be composed by the following tokens, separated by comma:\n"
"  <addr>[-<addr>]  Host address(es)\n"
"  ttl=<ttl>        TTL for fake host(s), default: 64\n"
"  mtu=<mtu>        MTU for fake host(s), default: 1500\n"
"                   Packets large than <mtu> will be silently dropped.\n"
"  route=<network>  Route for this chain, default: 0/0\n"
"                   If multiple chains with same route, it is undefined which\n"
"                   chain will be selected.\n"
"  TTL and MTU will take effect until next TTL/MTU specification.\n"
"\n"
"  -4 <v4addr_chain>       IPv4 address chain\n"
"  -6 <v6addr_chain>       IPv6 address chain\n"
"  -E <step>/<prefix>,<n>  duplicates the previous chain by <n>, with interval of\n"
"                          <step>*2^<prefix>. All route and hosts will be shifted\n"
"  -D                      daemonize (run in background)\n"
"  -d                      enables debugging messages\n"
"  -h                      prints this help text\n", stderr);
}


int main (int argc, char *argv[]) {
  if (argc <= 1) {
    usage(argv[0]);
    return EXIT_SUCCESS;
  }

  struct HostChain *v4_chains = NULL;
  struct HostChain *v6_chains = NULL;
  unsigned int v4_chains_len = 0;
  unsigned int v6_chains_len = 0;
  char if_name[IF_NAMESIZE] = RDNSTUN_IFACE_NAME;
  bool background = false;

  // Parse command line options
  bool if_name_set = false;
  bool last_chain_v6 = false;
  for (int option; (option = getopt(argc, argv, "-4:6:E:Ddh")) != -1;) {
    int ret;
    switch (option) {
      case 1:
        should (!if_name_set) otherwise {
          fprintf(stderr, "error: too many positional options\n");
          goto fail_arg;
        }
        if_name_set = true;
        should (strnlen(optarg, sizeof(if_name)) < sizeof(if_name)) otherwise {
          fprintf(stderr, "error: iface name '%s' too long\n", optarg);
          goto fail_arg;
        }
        strcpy(if_name, optarg);
        break;
      case '4': {
        test_goto (irealloc(
          (void **) &v4_chains, sizeof(struct HostChain) * (v4_chains_len + 2)
        ) != NULL, -1) fail_chain;
        goto_nonzero (
          HostChain_init(v4_chains + v4_chains_len, optarg, false)) fail_chain;
        v4_chains_len++;
        last_chain_v6 = false;
        break;
      }
      case '6': {
        test_goto (irealloc(
          (void **) &v6_chains, sizeof(struct HostChain) * (v6_chains_len + 2)
        ) != NULL, -1) fail_chain;
        goto_nonzero (
          HostChain_init(v6_chains + v6_chains_len, optarg, true)) fail_chain;
        v6_chains_len++;
        last_chain_v6 = true;
        break;
      }
      case 'E': {
        test_goto (
          (last_chain_v6 ? v6_chains_len : v4_chains_len) > 0, 1
        ) fail_duplicate;
        char *step_end = strchr(optarg, '/');
        char *prefix_end = strchr(optarg, ',');
        test_goto (step_end != NULL && prefix_end != NULL, 2) fail_duplicate;

        *step_end = '\0';
        *prefix_end = '\0';
        int step;
        int prefix;
        int n;
        bool parsed_int =
          argtoi(optarg, &step, 1, SHRT_MAX) == 0 &&
          argtoi(step_end + 1, &prefix, 1, last_chain_v6 ? 128 : 32) == 0 &&
          argtoi(prefix_end + 1, &n, 0, INT_MAX) == 0;
        *step_end = '/';
        *prefix_end = ',';
        test_goto (parsed_int, 2) fail_duplicate;

        test_goto (irealloc(
          (void **) (last_chain_v6 ? &v6_chains : &v4_chains),
          sizeof(struct HostChain) * (
            (last_chain_v6 ? v6_chains_len : v4_chains_len) + n + 1)
        ) != NULL, -1) fail_duplicate;
        struct HostChain *base = last_chain_v6 ?
          v6_chains + v6_chains_len - 1 : v4_chains + v4_chains_len - 1;
        test_goto (prefix <= base->prefix, 3) fail_duplicate;

        const int af = last_chain_v6 ? AF_INET6 : AF_INET;
        for (int i = 1; i <= n; i++) {
          struct HostChain *self = base + i;
          goto_nonzero (HostChain_copy(self, base)) fail_duplicate;
          HostChain_shift(self, step * i, prefix);

          if (logger_would_log(LOG_LEVEL_DEBUG)) {
            char s_network[INET6_ADDRSTRLEN];
            inet_ntop(af, self->network, s_network, sizeof(s_network));
            LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "Duplicate # %d chain: %s/%d",
                   i, s_network, self->prefix);
          }

          if (last_chain_v6) {
            v6_chains_len++;
          } else {
            v4_chains_len++;
          }
        }
        break;
      }
      case 'D':
        background = true;
        break;
      case 'd':
        logger_set_level(LOG_LEVEL_DEBUG);
        break;
      case 'h':
        usage(argv[0]);
        goto end;
      default:
        // getopt already print error for us
        // fprintf(stderr, "error: unknown option '%c'\n", option);
        if (0) {
          const char *msg;
          if (0) {
fail_chain:
            msg = HostChain_strerror(ret);
          }
          if (0) {
fail_duplicate:
            switch (ret) {
              case 1:
                msg = "'E' must be specified after a chain";
                break;
              case 2:
                msg = "malformed duplication specification";
                break;
              case 3:
                msg = "'prefix' must be less or equal than the prefix of "
                      "previous chain";
                break;
              default:
                msg = Struct_strerror(ret);
            }
          }
          should (msg != NULL) otherwise {
            msg = "unknown error";
          }
          fprintf(stderr, "error when parsing '%s': %s\n", optarg, msg);
        }
fail_arg:
        goto fail;
    }
  }

  should (v4_chains_len != 0 || v6_chains_len != 0) otherwise {
    fprintf(stderr, "error: must specify at least one chain\n");
    goto fail;
  }
  if (v4_chains != NULL) {
    memset(v4_chains + v4_chains_len, 0, sizeof(struct HostChain));
    HostChainArray_sort(v4_chains);
  }
  if (v6_chains != NULL) {
    memset(v6_chains + v6_chains_len, 0, sizeof(struct HostChain));
    HostChainArray_sort(v6_chains);
  }

  // initialize tun/tap interface
  int tunfd = tun_alloc(if_name, IFF_TUN);
  goto_if_fail (tunfd >= 0) fail;
  LOGGER(RDNSTUN_NAME, LOG_LEVEL_INFO,
         "Successfully connected to interface %s", if_name);
  should (ifup(if_name) >= 0) otherwise {
    LOGGER(RDNSTUN_NAME, LOG_LEVEL_MESSAGE,
           "Failed to bring up interface %s", if_name);
  }

  // daemonize
  if (background) {
    should (daemon(0, 0) == 0) otherwise {
      perror("daemon");
      goto fail;
    }
  }

  // main loop
  if (!background) {
    LOGGER(RDNSTUN_NAME, LOG_LEVEL_MESSAGE, "Start " RDNSTUN_NAME);
  }
  signal(SIGINT, shutdown_rdnstun);
  rdnstun(
    tunfd, v4_chains_len != 0 ? v4_chains : NULL,
    v6_chains_len != 0 ? v6_chains : NULL, &rdnstun_shutdown);
  close(tunfd);

  int ret;
end:
  ret = EXIT_SUCCESS;
  if (0) {
fail:
    ret = EXIT_FAILURE;
  }
  if (v4_chains != NULL) {
    HostChainArray_destroy_size(v4_chains, v4_chains_len);
    free(v4_chains);
  }
  if (v6_chains != NULL) {
    HostChainArray_destroy_size(v6_chains, v6_chains_len);
    free(v6_chains);
  }
  return ret;
}
