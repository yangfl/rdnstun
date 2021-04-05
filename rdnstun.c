#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include "macro.h"
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
      perror("poll");
      continue;
    }
    if (pollres == 0) {
      continue;
    }

    LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, NULL);
    // data from tun/tap: read it
    unsigned char pkt_receive[IP_MAXPACKET];
    int pkt_receive_len = cread(tunfd, pkt_receive, sizeof(pkt_receive));
    should (pkt_receive_len > 0) otherwise {
      break;
    }
    LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
           "Read %d bytes from the interface", pkt_receive_len);

    unsigned char pkt_send[IP_MAXPACKET];
    unsigned short pkt_send_len;
    register unsigned char ipver = ((struct iphdr *) pkt_receive)->version;
    switch (ipver) {
      int ret;
      case 4:
        goto_if_fail (v4_chains != NULL) undefined_ipver;
        goto_nonzero (HostChain4Array_reply(
          v4_chains, (struct iphdr *) pkt_receive, pkt_receive_len,
          (struct iphdr *) pkt_send, &pkt_send_len
        )) fail_reply;
        break;
      case 6:
        goto_if_fail (v6_chains != NULL) undefined_ipver;
        goto_nonzero (HostChain6Array_reply(
          v6_chains, (struct ip6_hdr *) pkt_receive, pkt_receive_len,
          (struct ip6_hdr *) pkt_send, &pkt_send_len
        )) fail_reply;
        break;
      default:
        LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "Unknown IP version %d", ipver);
        if (0) {
undefined_ipver:
          LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
                 "Received IPv%d packet but no v%d chains defined",
                 ipver, ipver);
        }
        if (0) {
fail_reply:
          switch (ret) {
            case 11:
              LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "No host to reply");
              break;
            case 12:
              LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
                     "Received packet with TTL 0");
              break;
            case 13:
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
      int n_write = cwrite(tunfd, pkt_send, pkt_send_len);
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
"  <addr>[-<addr>]   Host address(es)\n"
"  ttl=<ttl>         TTL for fake host(s), default: 64\n"
"  mtu=<mtu>         MTU for fake host(s), default: 1500\n"
"                    Packets large than <mtu> will be silently dropped.\n"
"  route=<network>   Route for this chain, default: 0/0\n"
"                    If multiple chains with same route, which chain will be\n"
"                    selected is undefined.\n"
"  TTL and MTU are valid until next TTL/MTU specification."
"\n"
"  -4 <v4addr_chain> IPv4 address chain\n"
"  -6 <v6addr_chain> IPv6 address chain\n"
"  -D                daemonize (run in background)\n"
"  -d                enable debugging messages\n"
"  -h                prints this help text\n", stderr);
}


int main (int argc, char *argv[]) {
  if (argc == 1) {
    usage(argv[0]);
    return EXIT_SUCCESS;
  }

  struct HostChain *v4_chains = malloc(sizeof(struct HostChain));
  struct HostChain *v6_chains = malloc(sizeof(struct HostChain));
  should (v4_chains != NULL && v6_chains != NULL) otherwise {
    fprintf(stderr, "error: out of memory\n");
    free(v4_chains);
    free(v6_chains);
    return EXIT_FAILURE;
  }
  unsigned int v4_chains_len = 0;
  unsigned int v6_chains_len = 0;
  char if_name[IF_NAMESIZE] = RDNSTUN_IFACE_NAME;
  bool background = false;

  // Parse command line options
  bool if_name_set = false;
  for (int option; (option = getopt(argc, argv, "-4:6:Ddh")) != -1;) {
    int ret;
    switch (option) {
      case 1:
        if unlikely (if_name_set) {
          fprintf(stderr, "error: too many positional options\n");
          return EXIT_FAILURE;
        }
        if_name_set = true;
        strncpy(if_name, optarg, sizeof(if_name) - 1);
        if unlikely (strlen(optarg) + 1 > IF_NAMESIZE) {
          fprintf(stderr, "error: iface name '%s' too long\n", optarg);
          return EXIT_FAILURE;
        }
        break;
      case '4': {
        struct HostChain *new_v4_chains =
          realloc(v4_chains, sizeof(struct HostChain) * (v4_chains_len + 2));
        test_goto (new_v4_chains != NULL, -1) fail_chain;
        v4_chains = new_v4_chains;
        goto_nonzero (
          HostChain_init(v4_chains + v4_chains_len, optarg, false)
        ) fail_chain;
        v4_chains_len++;
        break;
      }
      case '6': {
        struct HostChain *new_v6_chains =
          realloc(v6_chains, sizeof(struct HostChain) * (v6_chains_len + 2));
        test_goto (new_v6_chains != NULL, -1) fail_chain;
        v6_chains = new_v6_chains;
        goto_nonzero (
          HostChain_init(v6_chains + v6_chains_len, optarg, true)
        ) fail_chain;
        v6_chains_len++;
        break;
      }
      case 'D':
        background = true;
        break;
      case 'd':
        effective_log_level = LOG_LEVEL_DEBUG;
        break;
      case 'h':
        usage(argv[0]);
        goto end;
      default:
        fprintf(stderr, "error: unknown option '%c'\n", option);
        goto fail;
    }
    if (0) {
fail_chain:
      fprintf(stderr, "error: %s\n", HostChain_strerror(ret));
      memset(v4_chains + v4_chains_len, 0, sizeof(struct HostChain));
      memset(v4_chains + v4_chains_len, 0, sizeof(struct HostChain));
      goto fail;
    }
  }
  memset(v4_chains + v4_chains_len, 0, sizeof(struct HostChain));
  memset(v6_chains + v6_chains_len, 0, sizeof(struct HostChain));
  should (v4_chains_len != 0 || v6_chains_len != 0) otherwise {
    fprintf(stderr, "error: must specify at least one chain\n");
    goto fail;
  }
  HostChainArray_sort(v4_chains);
  HostChainArray_sort(v6_chains);

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
    HostChainArray_destroy(v4_chains);
    free(v4_chains);
  }
  if (v6_chains != NULL) {
    HostChainArray_destroy(v6_chains);
    free(v6_chains);
  }
  return ret;
}
