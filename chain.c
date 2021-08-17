#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include "macro.h"
#include "utils.h"
#include "inet.h"
#include "log.h"
#include "host.h"
#include "rdnstun.h"
#include "chain.h"


#define HostChain_AT(self, i) \
  ((struct FakeHost *) ((self)->_buf + struct_size * (i)))


size_t HostChain_nitem (const struct HostChain *self) {
  const unsigned int struct_size =
    self->v6 ? sizeof(struct FakeHost6) : sizeof(struct FakeHost);
  unsigned int i;
  for (i = 0; !BaseFakeHost_isnull(HostChain_AT(self, i), self->v6); i++) { }
  return i;
}


int HostChain_compare (
    const struct HostChain * restrict self,
    const struct HostChain * restrict other) {
  return_nonzero (cmp(other->prefix, self->prefix));
  return memcmp(self->network, other->network, (self->v6 ? 128 : 32) / 4);
}


bool HostChain_in (
    const struct HostChain * restrict self, const void * restrict addr) {
  return self->prefix == 0 || membcmp(self->network, addr, self->prefix) == 0;
}


void *HostChain_find (
    const struct HostChain * restrict self, const void * restrict addr,
    unsigned char ttl, bool *found, unsigned char *index) {
  const unsigned int struct_size =
    self->v6 ? sizeof(struct FakeHost6) : sizeof(struct FakeHost);
  unsigned int i;
  for (i = 0; ttl > 0 && !BaseFakeHost_isnull(HostChain_AT(self, i), self->v6);
       i++, ttl--) {
    if (self->v6 ?
          IN6_ARE_ADDR_EQUAL(&self->v6_chain[i].addr, addr) :
          self->v4_chain[i].addr.s_addr == ((struct in_addr *) addr)->s_addr) {
      *found = true;
      goto end;
    }
  }
  return_if (i == 0) NULL;
  *found = false;
  i--;
end:
  *index = i;
  return HostChain_AT(self, i);
}


int HostChain_shift (
    struct HostChain *self, int offset, unsigned short prefix) {
  const int af = self->v6 ? AF_INET6 : AF_INET;
  const unsigned int struct_size =
    self->v6 ? sizeof(struct FakeHost6) : sizeof(struct FakeHost);
  return_if_fail (inet_shift(af, self->network, offset, prefix) == 0) 14;
  for (unsigned int i = 0;
       !BaseFakeHost_isnull(HostChain_AT(self, i), self->v6); i++) {
    inet_shift(af, &HostChain_AT(self, i)->addr, offset, prefix);
  }
  return 0;
}


const char *HostChain_strerror (int errnum) {
  switch (errnum) {
    case 1:
      return "address not in presentation format";
    case 2:
      return "address range too large";
    case 3:
      return "address chain longer than current TTL";
    case 4:
      return "route not in presentation format";
    case 5:
      return "TTL not a number or out of range (make chain unreachable)";
    case 6:
      return "MTU not a number or out of range";
    case 7:
      return "route can only be specified once";
    case 8:
      return "TTL can only become smaller";
    case 9:
      return "MTU can only become smaller";
    case 10:
      return "chain is empty";
    case 11:
      return "No host to reply";
    case 12:
      return "TTL is zero";
    case 13:
      return "Host TTL too small";
    case 14:
      return "'prefix' must be less or equal than the network prefix";
    default:
      return Struct_strerror(errnum);
  }
}


void HostChain_destroy (struct HostChain *self) {
  free(self->_buf);
}


int HostChain_copy (
    struct HostChain * restrict self, const struct HostChain * restrict other) {
  const unsigned int struct_size =
    other->v6 ? sizeof(struct FakeHost6) : sizeof(struct FakeHost);
  memcpy(self, other, sizeof(struct HostChain));
  unsigned int len = HostChain_nitem(other) + 1;
  self->_buf = malloc(struct_size * len);
  return_if_fail (self->_buf != NULL) -1;
  memcpy(self->_buf, other->_buf, struct_size * len);
  return 0;
}


int HostChain_init (
    struct HostChain * restrict self, const char * restrict s, bool v6) {
  const unsigned int struct_size =
    v6 ? sizeof(struct FakeHost6) : sizeof(struct FakeHost);
  const int af = v6 ? AF_INET6 : AF_INET;

  int ret;
  self->_buf = malloc(struct_size * MAXTTL);
  char *s_ = strdup(s);
  test_goto (self->_buf != NULL && s_ != NULL, -1) fail;
  self->v6 = v6;

  bool route_set = false;
  unsigned int ttl = 0;
  unsigned int mtu = 0;
  unsigned int i = 0;
  for (char *saved_comma, *token = strtok_r(s_, ",", &saved_comma);
       token != NULL;
       token = strtok_r(NULL, ",", &saved_comma)) {
#define stracmp(s, l) strncmp(s, l, strlen(l))
    if (stracmp(token, "ttl=") == 0) {
      token += strlen("ttl=");
      int new_ttl;
      test_goto (argtoi(token, &new_ttl, i + 1, MAXTTL) == 0, 5) fail;
      ttl = new_ttl;
    } else if (stracmp(token, "mtu=") == 0) {
      token += strlen("mtu=");
      int new_mtu;
      test_goto (argtoi(
        token, &new_mtu, v6 ? 1280 : 576, mtu == 0 ? IP_MAXPACKET : mtu
      ) == 0, 6) fail;
      mtu = new_mtu;
    } else if (stracmp(token, "route=") == 0) {
      test_goto (!route_set, 7) fail;
      route_set = true;
      // save prefix len
      char *network_end = strchr(token, '/');
      test_goto (network_end != NULL, 4) fail;
      *network_end = '\0';
      // network
      token += strlen("route=");
      test_goto (inet_pton(af, token, self->network) == 1, 4) fail;
      // prefix len
      int prefix;
      test_goto (
        argtoi(network_end + 1, &prefix, 0, v6 ? 128 : 32) == 0, 4) fail;
      self->prefix = prefix;
      // verify cidr
      test_goto (
        inet_isnetwork(af, self->network, self->prefix) == 1, 4) fail;

      if (LogLevel_should_log(LOG_LEVEL_DEBUG)) {
        if (network_end != NULL) {
          *network_end = '/';
        }
        char s_network[INET6_ADDRSTRLEN];
        inet_ntop(af, self->network, s_network, sizeof(s_network));
        LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
               "Parsed route '%s': %s/%d", token, s_network, self->prefix);
      }
    } else {
      if (i == 0) {
        if (ttl == 0) {
          ttl = 64;
        }
        if (mtu == 0) {
          mtu = 1500;
        }
      } else {
        test_goto (i < ttl, 3) fail;
      }
      unsigned int old_i = i;

      char *next_dash = strchr(token, '-');
      if (next_dash != NULL) {
        *next_dash = '\0';
      }

      // parse first component
      struct FakeHost *host = HostChain_AT(self, i);
      test_goto (inet_pton(af, token, &host->addr) == 1, 1) fail;
      host->ttl = ttl;
      host->mtu = mtu;
      i++;

      // parse second component
      if (next_dash != NULL) {
        unsigned char addr_end[sizeof(struct in6_addr)];
        test_goto (inet_pton(af, next_dash + 1, addr_end) == 1, 1) fail;

        if likely (memcmp(
            &host->addr, addr_end,
            v6 ? sizeof(struct in6_addr) : sizeof(struct in_addr)) != 0) {
          register const int prefix_len =
            sizeof(struct in6_addr) - sizeof(struct in_addr);
          unsigned char prefix[prefix_len];
          in_addr_t start;
          in_addr_t stop;
          if (v6) {
            // compare the leading 96 (128-32) bits
            // if mismatched, the range must be too large (>= 256)
            test_goto (memcmp(
              ((struct FakeHost6 *) host)->addr.s6_addr,
              ((struct in6_addr *) addr_end)->s6_addr,
              prefix_len) == 0, 2) fail;
            memcpy(prefix, ((struct in6_addr *) addr_end)->s6_addr,
                   sizeof(prefix));
            start = ntohl(*((in_addr_t *) (
              ((struct FakeHost6 *) host)->addr.s6_addr + prefix_len)));
            stop = ntohl(*((in_addr_t *) (
              ((struct in6_addr *) addr_end)->s6_addr + prefix_len)));
          } else {
            start = ntohl(host->addr.s_addr);
            stop = ntohl(((struct in_addr *) addr_end)->s_addr);
          }
          // assert(start != stop)
          i--;
          int step = stop > start ? 1 : -1;
          unsigned int n_addr = (stop - start) * step + 1;
          test_goto (i + n_addr < MAXTTL, 2) fail;
          for (unsigned int j = 1; j < n_addr; j++) {
            struct FakeHost *host_j = HostChain_AT(self, i + j);
            if (v6) {
              memcpy(((struct FakeHost6 *) host_j)->addr.s6_addr, prefix,
                     sizeof(prefix));
              *((in_addr_t *) (
                  ((struct FakeHost6 *) host_j)->addr.s6_addr + prefix_len)) =
                htonl(start + j * step);
            } else {
              host_j->addr.s_addr = htonl(start + j * step);
            }
            host_j->ttl = ttl;
            host_j->mtu = mtu;
          }
          i += n_addr;
        }
      }

      if (LogLevel_should_log(LOG_LEVEL_DEBUG)) {
        if (next_dash != NULL) {
          *next_dash = '-';
        }
        LOGGER_START(RDNSTUN_NAME, LOG_LEVEL_DEBUG,
                     "Parsed address '%s': ", token);
        for (unsigned int j = old_i; j < i; j++) {
          char s_addr[INET6_ADDRSTRLEN];
          inet_ntop( af, &HostChain_AT(self, j)->addr, s_addr, sizeof(s_addr));
          if (j != old_i) {
            logger_continue_literal(LOG_LEVEL_DEBUG, ", ");
          }
          logger_continue_literal(LOG_LEVEL_DEBUG, s_addr);
        }
        logger_end(LOG_LEVEL_DEBUG);
      }
    }
  }
  test_goto (i != 0, 10) fail;

  free(s_);
  self->_buf = realloc(self->_buf, struct_size * (i + 1));
  memset(self->_buf + struct_size * i, 0, struct_size);
  return 0;

fail:
  free(self->_buf);
  free(s_);
  return ret;
}


void *HostChainArray_find (
    const struct HostChain * restrict self, const void * restrict addr,
    unsigned char ttl, unsigned char *index) {
  void *ret = NULL;
  for (unsigned int i = 0; self[i]._buf != NULL; i++) {
    if (HostChain_in(self + i, addr)) {
      bool found = false;  // bug -Wuninitialized
      void *host = HostChain_find(self + i, addr, ttl, &found, index);
      if unlikely (host == NULL) {
        continue;
      }
      ret = host;
      if (found) {
        break;
      }
    }
  }
  return ret;
}


int HostChain4Array_reply (
    const struct HostChain * restrict self,
    const struct iphdr * restrict receive, unsigned short receive_len,
    struct iphdr * restrict send, unsigned short * restrict send_len) {
  return_if_fail (receive->ttl > 0) 12;
  unsigned char index = 0;  // bug -Wuninitialized
  const struct FakeHost *host = HostChainArray_find(
    self, &receive->daddr, receive->ttl, &index);
  return_if_fail (host != NULL) 11;
  int ret = FakeHost_reply(host, receive, receive_len, index, send, send_len);
  if (ret > 0) {
    ret += 12;
  }
  return ret;
}


int HostChain6Array_reply (
    const struct HostChain * restrict self,
    const struct ip6_hdr * restrict receive, unsigned short receive_len,
    struct ip6_hdr * restrict send, unsigned short * restrict send_len) {
  return_if_fail (receive->ip6_hlim > 0) 12;
  unsigned char index = 0;  // bug -Wuninitialized
  const struct FakeHost6 *host = HostChainArray_find(
    self, &receive->ip6_dst, receive->ip6_hlim, &index);
  return_if_fail (host != NULL) 11;
  int ret = FakeHost6_reply(host, receive, receive_len, index, send, send_len);
  if (ret > 0) {
    ret += 12;
  }
  return ret;
}


size_t HostChainArray_nitem (const struct HostChain *self) {
  unsigned int i;
  for (i = 0; self[i]._buf != NULL; i++) { }
  return i;
}


void HostChainArray_sort (struct HostChain *self) {
  qsort(
    self, HostChainArray_nitem(self), sizeof(struct HostChain),
    (int (*) (const void *, const void *)) HostChain_compare);
}


void HostChainArray_destroy_size (struct HostChain *self, size_t n) {
  for (unsigned int i = 0; i < n; i++) {
    promise (self[i]._buf != self[i + 1]._buf);
    HostChain_destroy(self + i);
  }
}


void HostChainArray_destroy (struct HostChain *self) {
  for (unsigned int i = 0; self[i]._buf != NULL; i++) {
    promise (self[i]._buf != self[i + 1]._buf);
    HostChain_destroy(self + i);
  }
}
