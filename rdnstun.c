#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <sys/ioctl.h>

#include "macro.h"
#include "log.h"
#include "checksum.h"

#include "rdnstun.h"


/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            needs to reserve enough space in *dev.                      *
 **************************************************************************/
int tun_alloc (char dev[IFNAMSIZ], int flags) {
  int fd = open("/dev/net/tun", O_RDWR);
  should (fd >= 0) otherwise {
    perror("Error when opening /dev/net/tun");
    return fd;
  }

  struct ifreq ifr = {
    .ifr_flags = flags | IFF_NO_PI,
  };
  if (dev != NULL) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  int err = ioctl(fd, TUNSETIFF, &ifr);
  should (err >= 0) otherwise {
    perror("tun_alloc: ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  if (dev != NULL) {
    strcpy(dev, ifr.ifr_name);
  }

  return fd;
}


int ifup (const char ifname[]) {
  static int fd = -1;
  if (fd < 0) {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    should (fd >= 0) otherwise {
      perror("ifup: socket(SOCK_DGRAM)");
      return fd;
    }
  }

  struct ifreq ifr = {
    .ifr_flags = IFF_UP,
  };
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  int err = ioctl(fd, SIOCSIFFLAGS, &ifr);
  should (err >= 0) otherwise {
    perror("ifup: ioctl(SIOCSIFFLAGS)");
    return err;
  }

  return 0;
}


/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread (int fd, char buf[], int n){
  int nread = read(fd, buf, n);
  should (nread > 0) otherwise {
    perror("Reading data");
  }
  return nread;
}


/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
int cwrite (int fd, const char buf[], int n){
  int nwrite = write(fd, buf, n);
  should (nwrite >= 0) otherwise {
    perror("Writing data");
  }
  return nwrite;
}


// struct in_addr chain[MAXTTL + 1]
void *inet_chain_pton (int af, char arg[], void *chain) {
  const size_t struct_size =
    af == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr);

  int i = 0;
  for (char *saved_comma, *token = strtok_r(arg, ",", &saved_comma);
       token != NULL;
       i++, token = strtok_r(NULL, ",", &saved_comma)) {
    int token_error;

    should (i < MAXTTL) otherwise {
      token_error = 3;
      goto fail;
    }

    int old_i = i;

    char *next_dash = strchr(token, '-');
    if (next_dash != NULL) {
      *next_dash = '\0';
    }

    // parse first component
    token_error = inet_pton(af, token, (char *) chain + struct_size * i) <= 0;
    goto_if_fail (token_error == 0) fail;

    // parse second component
    if (next_dash != NULL) {
      *next_dash = '-';

      char addr_end[sizeof(struct in6_addr)];
      token_error = inet_pton(af, next_dash + 1, addr_end) <= 0;
      goto_if_fail (token_error == 0) fail;

      if (af == AF_INET) {
        in_addr_t start = ntohl(((struct in_addr *) chain)[i].s_addr);
        in_addr_t stop = ntohl(((struct in_addr *) addr_end)->s_addr);
        if (start != stop) {
          int step = stop > start ? 1 : -1;
          int n_addr = (stop - start) * step + 1;
          should (i + n_addr < MAXTTL) otherwise {
            token_error = 2;
            goto fail;
          }

          for (int j = 1; j < n_addr; j++) {
            ((struct in_addr *) chain)[i + j].s_addr = htonl(start + j * step);
          }
          i += n_addr - 1;
        }
      } else {
        // compare the leading 96 (128-32) bits
        // if mismatched, the range must be too large (>= 256)
        const int prefix_len = sizeof(struct in6_addr) - sizeof(struct in_addr);
        if (memcmp(((struct in6_addr *) chain)[i].s6_addr,
                   ((struct in6_addr *) addr_end)->s6_addr,
                   prefix_len) != 0) {
          token_error = 2;
          goto fail;
        }

        unsigned char prefix[prefix_len];
        memcpy(prefix, ((struct in6_addr *) chain)[i].s6_addr,
               sizeof(prefix));
        in_addr_t start = ntohl(*((in_addr_t *) (
          ((struct in6_addr *) chain)[i].s6_addr + prefix_len)));
        in_addr_t stop = ntohl(*((in_addr_t *) (
          ((struct in6_addr *) addr_end)->s6_addr + prefix_len)));
        if (start != stop) {
          int step = stop > start ? 1 : -1;
          int n_addr = (stop - start) * step + 1;
          should (i + n_addr < MAXTTL) otherwise {
            token_error = 2;
            goto fail;
          }

          for (int j = 1; j < n_addr; j++) {
            memcpy(((struct in6_addr *) chain)[i + j].s6_addr, prefix,
                   sizeof(prefix));
            *((in_addr_t *) (((struct in6_addr *) chain)[i + j].s6_addr
                             + prefix_len)) = htonl(start + j * step);
          }
          i += n_addr - 1;
        }
      }
    }

    if (0) {
fail:
      switch (token_error) {
        case 1:
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR,
                 "Address '%s' not in presentation format", token);
          break;
        case 2:
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR,
                 "Address range '%s' too large", token);
          break;
        case 3:
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR,
                 "Address chain already too long at '%s'", token);
          break;
      }
      return NULL;
    }

    if (cur_level >= LOG_LEVEL_DEBUG) {
      printf("Parsed address '%s': ", token);
      for (int j = old_i; j <= i; j++) {
        char s_addr[INET6_ADDRSTRLEN];
        inet_ntop(af, (char *) chain + struct_size * j, s_addr, sizeof(s_addr));
        if (j != old_i) {
          printf(", ");
        }
        printf("%s", s_addr);
      }
      printf("\n");
    }
  }

  memset((char *) chain + struct_size * i, 0, struct_size);
  return chain;
}


/**************************************************************************
 * usage: prints usage and exits.                                         *
 **************************************************************************/
void usage (const char *progname) {
  fprintf(stderr,
"Usage: %s [OPTIONS]... [<iface>]\n", progname);
  fputs(
"\n"
"Address chain may have the following format:\n"
"  <addr>[-<addr>],...\n"
"\n"
"  -4 <v4addr_chain> IPv4 address chain\n"
"  -6 <v6addr_chain> IPv6 address chain\n"
"  -t <ttl>          TTL for fake hosts\n"
"                    This parameter is not checked against the chain length.\n"
"  -m <mtu>          MTU for fake hosts\n"
"                    Packets large than <mtu> will be silently dropped.\n"
"  -D                daemonize (run in background)\n"
"  -d                enable debugging messages\n"
"  -h                prints this help text\n", stderr);
}


bool argtol (const char optarg[], long *res, long min, long max,
             const char argname[], const char log_domain[]) {
  char *optarg_end;
  *res = strtol(optarg, &optarg_end, 10);
  should (*optarg_end == '\0') otherwise {
    logger(log_domain, LOG_LEVEL_ERROR,
           "%s '%s' not a number", argname, optarg);
    return false;
  }
  should (min <= *res && *res <= max) otherwise {
    logger(log_domain, LOG_LEVEL_ERROR,
           "%s '%s' out of range", argname, optarg);
    return false;
  }
  return true;
}


int main (int argc, char *argv[]) {
  if (argc == 1) {
    usage(argv[0]);
    return EXIT_SUCCESS;
  }

  char if_name[IFNAMSIZ] = RDNSTUN_IFACE_NAME;
  struct in_addr v4_chain[MAXTTL + 1];
  struct in6_addr v6_chain[MAXTTL + 1];
  u_char host_ttl = 64;
  u_short host_mtu = 1500;
  bool background = false;

  /* Parse command line options */
  bool if_name_set = false;
  bool v4_chain_set = false;
  bool v6_chain_set = false;

  for (int option; (option = getopt(argc, argv, "-4:6:t:m:Ddh")) != -1;) {
    switch (option) {
      case 1:
        if unlikely (if_name_set) {
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR, "Too many positional options!");
          return EXIT_FAILURE;
        }
        if_name_set = true;
        strncpy(if_name, optarg, sizeof(if_name) - 1);
        if unlikely (strlen(optarg) + 1 > IFNAMSIZ) {
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR,
                 "Iface name '%s' too long!", optarg);
          return EXIT_FAILURE;
        }
        break;
      case '4':
        if unlikely (v4_chain_set) {
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR,
                 "Duplicated v4 chain '%s'", optarg);
          return EXIT_FAILURE;
        }
        v4_chain_set = true;
        should (inet_chain_pton(AF_INET, optarg, v4_chain) != NULL) otherwise {
          return EXIT_FAILURE;
        }
        break;
      case '6':
        if unlikely (v6_chain_set) {
          logger(RDNSTUN_NAME, LOG_LEVEL_ERROR,
                 "Duplicated v6 chain '%s'", optarg);
          return EXIT_FAILURE;
        }
        v6_chain_set = true;
        should (inet_chain_pton(AF_INET6, optarg, v6_chain) != NULL) otherwise {
          return EXIT_FAILURE;
        }
        break;
      case 't': {
        long host_ttl_;
        if (!argtol(optarg, &host_ttl_, 1, MAXTTL, "TTL", RDNSTUN_NAME)) {
          return EXIT_FAILURE;
        }
        host_ttl = host_ttl_;
        break;
      }
      case 'm': {
        long host_mtu_;
        if (!argtol(optarg, &host_mtu_, 1200, IP_MAXPACKET, "MTU",
                    RDNSTUN_NAME)) {
          return EXIT_FAILURE;
        }
        host_mtu = host_mtu_;
        break;
      }
      case 'D':
        background = true;
        break;
      case 'd':
        cur_level = LOG_LEVEL_DEBUG;
        break;
      case 'h':
        usage(argv[0]);
        return EXIT_SUCCESS;
      default:
        logger(RDNSTUN_NAME, LOG_LEVEL_ERROR, "Unknown option '%c'", option);
        return EXIT_FAILURE;
    }
  }

  if unlikely (!v4_chain_set && !v6_chain_set){
    logger(RDNSTUN_NAME, LOG_LEVEL_ERROR, "Must specify at least one chain!");
    return EXIT_FAILURE;
  }

  /* initialize tun/tap interface */
  int tun_fd = tun_alloc(if_name, IFF_TUN);
  should (tun_fd >= 0) otherwise {
    return EXIT_FAILURE;
  }
  logger(RDNSTUN_NAME, LOG_LEVEL_INFO,
         "Successfully connected to interface %s", if_name);
  should (ifup(if_name) >= 0) otherwise {
    logger(RDNSTUN_NAME, LOG_LEVEL_MESSAGE,
           "Failed to bring up interface %s", if_name);
  }

  if (background) {
    should (daemon(0, 0) == 0) otherwise {
      perror("daemon");
      return EXIT_FAILURE;
    }
  }

  /* main loop */
  struct pollfd fds[] = {
    {.fd = tun_fd, .events = POLLIN},
  };

  while (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) > 0) {
    /* data from tun/tap: read it */
    char pkt_receive_[IP_MAXPACKET];
    int pkt_receive_len = cread(tun_fd, pkt_receive_, sizeof(pkt_receive_));
    should (pkt_receive_len > 0) otherwise {
      break;
    }
    logger(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
           "Read %d bytes from the interface", pkt_receive_len);

    char pkt_send_[IP_MAXPACKET];
    size_t pkt_send_len = 0;
    switch (((struct ip *) pkt_receive_)->ip_v) {
      case IPVERSION: {
        if (!v4_chain_set) {
          break;
        }

        const struct ip *pkt_receive = (struct ip *) pkt_receive_;
        struct ip *pkt_send = (struct ip *) pkt_send_;

        // prepare send ip header
        memcpy(pkt_send, pkt_receive, sizeof(struct ip));
        pkt_send->ip_dst.s_addr = pkt_receive->ip_src.s_addr;

        // find reply host
        u_char pkt_ttl = pkt_receive->ip_ttl;
        if (pkt_ttl <= 0) {
          break;
        }
        int i_target;
        bool dst_is_target = false;
        for (i_target = 0;; i_target++, pkt_ttl--) {
          if (v4_chain[i_target].s_addr == INADDR_ANY) {
            // end of chain
            i_target--;
            pkt_ttl++;
            break;
          }
          if (v4_chain[i_target].s_addr == pkt_receive->ip_dst.s_addr) {
            dst_is_target = true;
            break;
          }
          if (pkt_ttl == 1) {
            break;
          }
        }
        if (cur_level >= LOG_LEVEL_DEBUG) {
          char s_dst_addr[INET_ADDRSTRLEN];
          char s_src_addr[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &pkt_receive->ip_dst,
                    s_dst_addr, sizeof(s_dst_addr));
          inet_ntop(AF_INET, v4_chain + i_target,
                    s_src_addr, sizeof(s_src_addr));
          printf("%s %d -> %s %d\n",
                 s_dst_addr, pkt_receive->ip_ttl, s_src_addr, pkt_ttl);
        }
        if (host_ttl <= i_target) {
          logger(RDNSTUN_NAME, LOG_LEVEL_INFO,
                 "Host TTL too small %d\n", host_ttl);
          break;
        }
        pkt_send->ip_ttl = host_ttl - i_target;
        pkt_send->ip_p = IPPROTO_ICMP;
        pkt_send->ip_src.s_addr = v4_chain[i_target].s_addr;

        // prepare reply
        const struct icmp *pkt_receive_icmp = (struct icmp *) (pkt_receive + 1);
        struct icmp *pkt_send_icmp = (struct icmp *) (pkt_send + 1);
        pkt_send_icmp->icmp_void = 0;
        if (pkt_ttl == 1 && !dst_is_target) {
          // not us and ttl exceeded
          pkt_send_icmp->icmp_type = ICMP_TIMXCEED;
          pkt_send_icmp->icmp_code = ICMP_TIMXCEED_INTRANS;
        } else if (!dst_is_target) {
          // not us and ttl not exceeded (at the end of chain), wrong route
          pkt_send_icmp->icmp_type = ICMP_UNREACH;
          pkt_send_icmp->icmp_code = ICMP_UNREACH_HOST;
        } else if (pkt_receive->ip_p == IPPROTO_ICMP &&
                   pkt_receive_icmp->icmp_type == ICMP_ECHO) {
          // ping
          pkt_send_icmp->icmp_type = ICMP_ECHOREPLY;
          pkt_send_icmp->icmp_code = 0;
          pkt_send_icmp->icmp_id = pkt_receive_icmp->icmp_id;
          pkt_send_icmp->icmp_seq = pkt_receive_icmp->icmp_seq;
          pkt_send_len = pkt_receive_len - sizeof(struct ip) - ICMP_MINLEN;
          memcpy(pkt_send_icmp->icmp_data, pkt_receive_icmp->icmp_data,
                 pkt_send_len);
          goto v4_no_copy_received_pkt;
        } else {
          // other packet, reply port closed
          pkt_send_icmp->icmp_type = ICMP_UNREACH;
          pkt_send_icmp->icmp_code = ICMP_UNREACH_PORT;
        }
        pkt_send_len =
          min(pkt_receive_len, host_mtu - sizeof(struct ip) - ICMP_MINLEN);
        memcpy(pkt_send_icmp->icmp_data, pkt_receive, pkt_send_len);
v4_no_copy_received_pkt:

        // fill icmp header checksum
        pkt_send_len += ICMP_MINLEN;
        inet_cksum(&pkt_send_icmp->icmp_cksum, pkt_send_icmp, pkt_send_len);

        // fill ip header checksum
        pkt_send_len += sizeof(struct ip);
        pkt_send->ip_len = htons(pkt_send_len);
        inet_cksum(&pkt_send->ip_sum, pkt_send, pkt_send_len);
        break;
      }
      case 6: {
        if (!v6_chain_set) {
          break;
        }

        const struct ip6_hdr *pkt_receive = (struct ip6_hdr *) pkt_receive_;
        struct ip6_hdr *pkt_send = (struct ip6_hdr *) pkt_send_;

        // prepare send ip header
        memcpy(pkt_send, pkt_receive, sizeof(struct ip6_hdr));
        memcpy(&pkt_send->ip6_dst, &pkt_receive->ip6_src,
               sizeof(struct in6_addr));

        // find reply host
        u_char pkt_ttl = pkt_receive->ip6_hlim;
        if (pkt_ttl <= 0) {
          break;
        }
        int i_target;
        bool dst_is_target = false;
        for (i_target = 0;; i_target++, pkt_ttl--) {
          if (IN6_IS_ADDR_UNSPECIFIED(v6_chain + i_target)) {
            // end of chain
            i_target--;
            pkt_ttl++;
            break;
          }
          if (IN6_ARE_ADDR_EQUAL(v6_chain + i_target, &pkt_receive->ip6_dst)) {
            dst_is_target = true;
            break;
          }
          if (pkt_ttl == 1) {
            break;
          }
        }
        if (cur_level >= LOG_LEVEL_DEBUG) {
          char s_dst_addr[INET6_ADDRSTRLEN];
          char s_src_addr[INET6_ADDRSTRLEN];
          inet_ntop(AF_INET6, &pkt_receive->ip6_dst,
                    s_dst_addr, sizeof(s_dst_addr));
          inet_ntop(AF_INET6, v6_chain + i_target,
                    s_src_addr, sizeof(s_src_addr));
          printf("%s %d -> %s %d\n",
                 s_dst_addr, pkt_receive->ip6_hlim, s_src_addr, pkt_ttl);
        }
        if (host_ttl <= i_target) {
          logger(RDNSTUN_NAME, LOG_LEVEL_INFO,
                 "Host TTL too small %d\n", host_ttl);
          break;
        }
        pkt_send->ip6_hlim = host_ttl - i_target;
        pkt_send->ip6_nxt = IPPROTO_ICMPV6;
        memcpy(&pkt_send->ip6_src, v6_chain + i_target,
               sizeof(struct in6_addr));

        // prepare reply
        const struct icmp6_hdr *pkt_receive_icmp =
          (struct icmp6_hdr *) (pkt_receive + 1);
        struct icmp6_hdr *pkt_send_icmp =
          (struct icmp6_hdr *) (pkt_send + 1);
        *pkt_send_icmp->icmp6_data32 = 0;
        if (pkt_ttl == 1 && !dst_is_target) {
          // not us and ttl exceeded
          pkt_send_icmp->icmp6_type = ICMP6_TIME_EXCEEDED;
          pkt_send_icmp->icmp6_code = ICMP6_TIME_EXCEED_TRANSIT;
        } else if (!dst_is_target) {
          // not us and ttl not exceeded (at the end of chain), wrong route
          pkt_send_icmp->icmp6_type = ICMP6_DST_UNREACH;
          pkt_send_icmp->icmp6_code = ICMP6_DST_UNREACH_ADDR;
        } else if (pkt_receive->ip6_nxt == IPPROTO_ICMPV6 &&
                   pkt_receive_icmp->icmp6_type == ICMP6_ECHO_REQUEST) {
          // ping
          pkt_send_icmp->icmp6_type = ICMP6_ECHO_REPLY;
          pkt_send_icmp->icmp6_code = 0;
          *pkt_send_icmp->icmp6_data32 = *pkt_receive_icmp->icmp6_data32;
          pkt_send_len =
            pkt_receive_len - sizeof(struct ip6_hdr) - sizeof(struct icmp6_hdr);
          memcpy(pkt_send_icmp + 1, pkt_receive_icmp + 1, pkt_send_len);
          goto v6_no_copy_received_pkt;
        } else {
          // other packet, reply port closed
          pkt_send_icmp->icmp6_type = ICMP6_DST_UNREACH;
          pkt_send_icmp->icmp6_code = ICMP6_DST_UNREACH_NOPORT;
        }
        pkt_send_len = min(pkt_receive_len, host_mtu - sizeof(struct ip6_hdr) -
                                            sizeof(struct icmp6_hdr));
        memcpy(pkt_send_icmp + 1, pkt_receive, pkt_send_len);
v6_no_copy_received_pkt:

        // fill icmp header checksum
        pkt_send_len += sizeof(struct icmp6_hdr);
        pkt_send_icmp->icmp6_cksum = 0;
        uint32_t sum = inet_cksum_continue(0, &pkt_send->ip6_src, 32);
        sum += htonl(pkt_send_len);
        sum += htonl(pkt_send->ip6_nxt);
        sum = inet_cksum_continue(sum, pkt_send_icmp, pkt_send_len);
        pkt_send_icmp->icmp6_cksum = inet_cksum_finish(sum);

        // fill ip header checksum
        pkt_send->ip6_plen = htons(pkt_send_len);
        pkt_send_len += sizeof(struct ip6_hdr);
        break;
      }
      default:
        logger(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
               "Unknown IP version %d", ((struct ip *) pkt_receive_)->ip_v);
    }

    /* write it into the tun/tap interface */
    if (pkt_send_len > 0) {
      int n_write = cwrite(tun_fd, pkt_send_, pkt_send_len);
      logger(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
             "Written %d bytes to the interface", n_write);
    }
    logger(RDNSTUN_NAME, LOG_LEVEL_DEBUG, " ");
  }

  return EXIT_SUCCESS;
}
