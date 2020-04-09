#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
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
#include <sys/socket.h>

#include "macro.h"
#include "tinyglib.h"
#include "checksum.h"

#include "rdnstun.h"


/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            needs to reserve enough space in *dev.                      *
 **************************************************************************/
int tun_alloc (char *dev, size_t size) {
  int fd = open("/dev/net/tun", O_RDWR);
  should (fd >= 0) otherwise {
    perror("Error when opening /dev/net/tun");
    return fd;
  }

  struct ifreq ifr = {
    .ifr_flags = IFF_TUN | IFF_NO_PI,
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
    strncpy(dev, ifr.ifr_name, size);
  }

  return fd;
}


/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread (int fd, char *buf, int n){
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
int cwrite (int fd, char *buf, int n){
  int nwrite = write(fd, buf, n);
  should (nwrite >= 0) otherwise {
    perror("Writing data");
  }
  return nwrite;
}


/**************************************************************************
 * read_n: ensures we read exactly n bytes, and puts those into "buf".    *
 *         (unless EOF, of course)                                        *
 **************************************************************************/
int read_n (int fd, char *buf, int n) {
  for (int i = 0; i < n; ) {
    int nread = cread(fd, buf + i, n - i);
    should (nread > 0) otherwise {
      return 0;
    }
    i += nread;
  }
  return n;
}


void *inet_chain_pton (int af, char *arg) {
  size_t struct_size =
    af == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr);
  GArray *chain = g_array_new(false, false, struct_size);

  bool error = 0;
  int i = 0;
  for (char *saved_comma, *token = strtok_r(arg, ",", &saved_comma);
       token != NULL;
       i++, token = strtok_r(NULL, ",", &saved_comma)) {
    int old_i = i;

    char *next_dash = strchr(token, '-');
    if (next_dash != NULL) {
      *next_dash = '\0';
    }

    // parse first component
    int token_error;
    g_array_set_size(chain, i + 1);
    token_error = inet_pton(af, token, chain->data + struct_size * i) <= 0;

    // parse second component
    if (next_dash != NULL && !token_error) do_once {
      *next_dash = '-';
      char addr_end[sizeof(struct in6_addr)];
      token_error = inet_pton(af, next_dash + 1, addr_end) <= 0;
      if (token_error) {
        break;
      }

      if (af == AF_INET) {
        in_addr_t start = ntohl(g_array_index(chain, struct in_addr, i).s_addr);
        in_addr_t stop = ntohl(((struct in_addr *) addr_end)->s_addr);
        if (start == stop) {
          break;
        }

        int step = stop > start ? 1 : -1;
        int n_addr = (stop - start) * step + 1;
        g_array_set_size(chain, i + n_addr);
        for (int j = 1; j < n_addr; j++) {
          g_array_index(chain, struct in_addr, i + j).s_addr =
            htonl(start + j * step);
        }
        i += n_addr - 1;
      } else {
        const int prefix_len = sizeof(struct in6_addr) - sizeof(struct in_addr);
        if (memcmp(g_array_index(chain, struct in6_addr, i).s6_addr,
                   ((struct in6_addr *) addr_end)->s6_addr,
                   prefix_len) != 0) {
          token_error = 2;
          break;
        }

        unsigned char prefix[prefix_len];
        memcpy(prefix, g_array_index(chain, struct in6_addr, i).s6_addr,
               sizeof(prefix));
        in_addr_t start = ntohl(*((in_addr_t *) (
          g_array_index(chain, struct in6_addr, i).s6_addr + prefix_len)));
        in_addr_t stop = ntohl(*((in_addr_t *) (
          ((struct in6_addr *) addr_end)->s6_addr + prefix_len)));
        if (start == stop) {
          break;
        }

        int step = stop > start ? 1 : -1;
        int n_addr = (stop - start) * step + 1;
        g_array_set_size(chain, i + n_addr);
        for (int j = 1; j < n_addr; j++) {
          memcpy(g_array_index(chain, struct in6_addr, i + j).s6_addr, prefix,
                 sizeof(prefix));
          *((in_addr_t *) (g_array_index(chain, struct in6_addr, i + j).s6_addr +
                           prefix_len)) = htonl(start + j * step);
        }
        i += n_addr - 1;
      }
    }

    if (token_error) {
      switch (token_error) {
        case 1:
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "Address '%s' not in presentation format", token);
          error = true;
          break;
        case 2:
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "Address range '%s' too large", token);
          break;
      }
      error = true;
    } else {
      if (cur_level >= G_LOG_LEVEL_DEBUG) {
        printf("Parsed address '%s': ", token);
        for (int j = old_i; j <= i; j++) {
          char s_addr[INET6_ADDRSTRLEN];
          inet_ntop(af, chain->data + struct_size * j, s_addr, sizeof(s_addr));
          if (j != old_i) {
            printf(", ");
          }
          printf("%s", s_addr);
        }
        printf("\n");
      }
    }
  }

  g_array_set_size(chain, chain->len + 1);
  memset(chain->data + struct_size * (chain->len - 1), 0, struct_size);
  void *ret = g_array_free(chain, false);
  return error ? NULL : ret;
}


/**************************************************************************
 * usage: prints usage and exits.                                         *
 **************************************************************************/
void usage (const char *progname) {
  fprintf(stderr,
"Usage: %s [OPTIONS]... [<iface>]\n", progname);
  fputs(
"\n"
"Addresses chain may have the following format:\n"
"  <addr>[-<addr>],...\n"
"\n"
"-4 <v4addr_chain>: IPv4 addresses chain\n"
"-6 <v6addr_chain>: IPv6 addresses chain\n"
"-t <ttl>: host ttl\n"
"-d: outputs debug information while running\n"
"-h: prints this help text\n", stderr);
}


int main (int argc, char *argv[]) {
  if (argc == 1) {
    usage(argv[0]);
    return EXIT_SUCCESS;
  }

  char if_name[IFNAMSIZ] = RDNSTUN_IFACE_NAME;
  struct in_addr *v4_chain = NULL;
  struct in6_addr *v6_chain = NULL;
  u_char host_ttl = 64;

  /* Check command line options */
  bool if_name_set = false;

  for (int option; (option = getopt(argc, argv, "-4:6:t:dh")) != -1;) {
    switch (option) {
      case 1:
        if unlikely (if_name_set) {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR, "Too many options!");
          return EXIT_FAILURE;
        }
        if_name_set = true;
        strncpy(if_name, optarg, sizeof(if_name) - 1);
        if unlikely (strlen(optarg) + 1 > IFNAMSIZ) {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "Iface name '%s' too long!", optarg);
          return EXIT_FAILURE;
        }
        break;
      case '4':
        if unlikely (v4_chain != NULL) {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "Duplicated v4 chain '%s'", optarg);
          return EXIT_FAILURE;
        }
        v4_chain = inet_chain_pton(AF_INET, optarg);
        should (v4_chain != NULL) otherwise {
          return EXIT_FAILURE;
        }
        break;
      case '6':
        if unlikely (v6_chain != NULL) {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "Duplicated v6 chain '%s'", optarg);
          return EXIT_FAILURE;
        }
        v6_chain = inet_chain_pton(AF_INET6, optarg);
        should (v6_chain != NULL) otherwise {
          return EXIT_FAILURE;
        }
        break;
      case 't': {
        char *optarg_end;
        long host_ttl_ = strtol(optarg, &optarg_end, 10);
        should (*optarg_end == '\0') otherwise {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "TTL '%s' not a number", optarg);
          return EXIT_FAILURE;
        }
        should (host_ttl_ > 0 && host_ttl_ <= MAXTTL) otherwise {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR,
                "TTL '%s' out of range", optarg);
          return EXIT_FAILURE;
        }
        host_ttl = host_ttl_;
        break;
      }
      case 'd':
        cur_level = G_LOG_LEVEL_DEBUG;
        break;
      case 'h':
        usage(argv[0]);
        return EXIT_SUCCESS;
      default:
        g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR, "Unknown option '%c'", option);
        return EXIT_FAILURE;
    }
  }

  if unlikely (v4_chain == NULL && v6_chain == NULL){
    g_log(RDNSTUN_NAME, G_LOG_LEVEL_ERROR, "Must specify at least one chain!");
    return EXIT_FAILURE;
  }

  /* initialize tun/tap interface */
  int tun_fd = tun_alloc(if_name, sizeof(if_name));
  should (tun_fd >= 0) otherwise {
    return EXIT_FAILURE;
  }
  g_log(RDNSTUN_NAME, G_LOG_LEVEL_INFO,
        "Successfully connected to interface %s", if_name);

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
    g_log(RDNSTUN_NAME, G_LOG_LEVEL_DEBUG,
          "Read %d bytes from the interface", pkt_receive_len);

    char pkt_send_[IP_MAXPACKET];
    size_t pkt_send_len = 0;
    switch (((struct ip *) pkt_receive_)->ip_v) {
      case IPVERSION: {
        if (v4_chain == NULL) {
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
        if (cur_level >= G_LOG_LEVEL_DEBUG) {
          char s_dst_addr[INET_ADDRSTRLEN];
          char s_src_addr[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &pkt_receive->ip_dst, s_dst_addr, sizeof(s_dst_addr));
          inet_ntop(AF_INET, v4_chain + i_target, s_src_addr, sizeof(s_src_addr));
          printf("%s %d -> %s %d\n", s_dst_addr, pkt_receive->ip_ttl, s_src_addr, pkt_ttl);
        }
        if (host_ttl <= i_target) {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_INFO,
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
          pkt_send_icmp->icmp_type = ICMP_TIMXCEED;
          pkt_send_icmp->icmp_code = ICMP_TIMXCEED_INTRANS;
        } else if (!dst_is_target) {
          pkt_send_icmp->icmp_type = ICMP_UNREACH;
          pkt_send_icmp->icmp_code = ICMP_UNREACH_HOST;
        } else if (pkt_receive->ip_p == IPPROTO_ICMP &&
                   pkt_receive_icmp->icmp_type == ICMP_ECHO) {
          pkt_send_icmp->icmp_type = ICMP_ECHOREPLY;
          pkt_send_icmp->icmp_code = 0;
          pkt_send_icmp->icmp_id = pkt_receive_icmp->icmp_id;
          pkt_send_icmp->icmp_seq = pkt_receive_icmp->icmp_seq;
          pkt_send_len = pkt_receive_len - sizeof(struct ip) - ICMP_MINLEN;
          memcpy(pkt_send_icmp->icmp_data, pkt_receive_icmp->icmp_data, pkt_send_len);
          goto no_copy_receive_v4;
        } else {
          pkt_send_icmp->icmp_type = ICMP_UNREACH;
          pkt_send_icmp->icmp_code = ICMP_UNREACH_PORT;
        }
        pkt_send_len = pkt_receive_len;
        memcpy(pkt_send_icmp->icmp_data, pkt_receive, pkt_send_len);

        // fill icmp header checksum
no_copy_receive_v4:
        pkt_send_len += ICMP_MINLEN;
        inet_cksum(&pkt_send_icmp->icmp_cksum, pkt_send_icmp, pkt_send_len);

        // fill ip header checksum
        pkt_send_len += sizeof(struct ip);
        pkt_send->ip_len = htons(pkt_send_len);
        inet_cksum(&pkt_send->ip_sum, pkt_send, pkt_send_len);
        break;
      }
      case 6: {
        if (v6_chain == NULL) {
          break;
        }

        const struct ip6_hdr *pkt_receive = (struct ip6_hdr *) pkt_receive_;
        struct ip6_hdr *pkt_send = (struct ip6_hdr *) pkt_send_;

        // prepare send ip header
        memcpy(pkt_send, pkt_receive, sizeof(struct ip6_hdr));
        memcpy(&pkt_send->ip6_dst, &pkt_receive->ip6_src, sizeof(struct in6_addr));

        // find reply host
        u_char pkt_ttl = pkt_receive->ip6_hlim;
        if (pkt_ttl <= 0) {
          break;
        }
        int i_target;
        bool dst_is_target = false;
        for (i_target = 0;; i_target++, pkt_ttl--) {
          if (IN6_IS_ADDR_UNSPECIFIED(v6_chain + i_target)) {
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
        if (cur_level >= G_LOG_LEVEL_DEBUG) {
          char s_dst_addr[INET6_ADDRSTRLEN];
          char s_src_addr[INET6_ADDRSTRLEN];
          inet_ntop(AF_INET6, &pkt_receive->ip6_dst, s_dst_addr, sizeof(s_dst_addr));
          inet_ntop(AF_INET6, v6_chain + i_target, s_src_addr, sizeof(s_src_addr));
          printf("%s %d -> %s %d\n", s_dst_addr, pkt_receive->ip6_hlim, s_src_addr, pkt_ttl);
        }
        if (host_ttl <= i_target) {
          g_log(RDNSTUN_NAME, G_LOG_LEVEL_INFO,
                "Host TTL too small %d\n", host_ttl);
          break;
        }
        pkt_send->ip6_hlim = host_ttl - i_target;
        pkt_send->ip6_nxt = IPPROTO_ICMPV6;
        memcpy(&pkt_send->ip6_src, v6_chain + i_target, sizeof(struct in6_addr));

        // prepare reply
        const struct icmp6_hdr *pkt_receive_icmp =
          (struct icmp6_hdr *) (pkt_receive + 1);
        struct icmp6_hdr *pkt_send_icmp =
          (struct icmp6_hdr *) (pkt_send + 1);
        *pkt_send_icmp->icmp6_data32 = 0;
        if (pkt_ttl == 1 && !dst_is_target) {
          pkt_send_icmp->icmp6_type = ICMP6_TIME_EXCEEDED;
          pkt_send_icmp->icmp6_code = ICMP6_TIME_EXCEED_TRANSIT;
        } else if (!dst_is_target) {
          pkt_send_icmp->icmp6_type = ICMP6_DST_UNREACH;
          pkt_send_icmp->icmp6_code = ICMP6_DST_UNREACH_ADDR;
        } else if (pkt_receive->ip6_nxt == IPPROTO_ICMPV6 &&
                   pkt_receive_icmp->icmp6_type == ICMP6_ECHO_REQUEST) {
          pkt_send_icmp->icmp6_type = ICMP6_ECHO_REPLY;
          pkt_send_icmp->icmp6_code = 0;
          *pkt_send_icmp->icmp6_data32 = *pkt_receive_icmp->icmp6_data32;
          pkt_send_len = pkt_receive_len - sizeof(struct ip6_hdr) - sizeof(struct icmp6_hdr);
          memcpy(pkt_send_icmp + 1, pkt_receive_icmp + 1, pkt_send_len);
          goto no_copy_receive_v6;
        } else {
          pkt_send_icmp->icmp6_type = ICMP6_DST_UNREACH;
          pkt_send_icmp->icmp6_code = ICMP6_DST_UNREACH_NOPORT;
        }
        pkt_send_len = pkt_receive_len;
        memcpy(pkt_send_icmp + 1, pkt_receive, pkt_send_len);

        // fill icmp header checksum
no_copy_receive_v6:
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
        g_log(RDNSTUN_NAME, G_LOG_LEVEL_DEBUG,
              "Unknown IP version %d", ((struct ip *) pkt_receive_)->ip_v);
    }

    /* write it into the tun/tap interface */
    if (pkt_send_len > 0) {
      int n_write = cwrite(tun_fd, pkt_send_, pkt_send_len);
      g_log(RDNSTUN_NAME, G_LOG_LEVEL_DEBUG,
            "Written %d bytes to the interface", n_write);
    }

    g_log(RDNSTUN_NAME, G_LOG_LEVEL_DEBUG, " ");
  }

  return EXIT_SUCCESS;
}
