#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "macro.h"
#include "endian.h"
#include "inet.h"


uint32_t inet_cksum_continue (uint32_t sum, const void *buf, size_t count) {
  const uint16_t *addr = buf;

  // Main summing loop
  for (; count > 1; count -= 2) {
    sum += *addr;
    addr++;
  }

  // Add left-over byte, if any
  if (count > 0) {
    sum += *((uint8_t *) addr);
  }

  return sum;
}


uint16_t _inet_cksum (void *cksum, const void *buf, size_t count, int append) {
  uint16_t *ip_cksum = cksum;
  // first, set cksum to 0
  if (ip_cksum != NULL) {
    *ip_cksum = 0;
  }
  // calculate cksum
  uint16_t sum = inet_cksum_finish(
    (append ? 0xffff : 0) + inet_cksum_continue(0, buf, count));
  // write cksum
  if (ip_cksum != NULL) {
    *ip_cksum = sum;
  }
  return sum;
}


uint16_t inet_cksum_header (void *cksum, const void *buf, size_t count) {
  return _inet_cksum(cksum, buf, count, 1);
}


uint16_t inet_cksum (void *cksum, const void *buf, size_t count) {
  return _inet_cksum(cksum, buf, count, 0);
}


int membcmp (const void *s1, const void *s2, size_t n) {
  unsigned int bytes = n / 8;
  unsigned int bits = n % 8;
  return_nonzero (memcmp(s1, s2, bytes));
  if (bits == 0) {
    return 0;
  }

  uint8_t mask = (uint8_t) -1 << (8 - bits);
  uint8_t c1 = ((const uint8_t *) s1)[bytes] & mask;
  uint8_t c2 = ((const uint8_t *) s2)[bytes] & mask;
  return cmp(c1, c2);
}

static unsigned short inet_size (int af) {
  switch (af) {
    case AF_INET6:
      return 128;
    case AF_INET:
      return 32;
    default:
      errno = EAFNOSUPPORT;
      return 0;
  }
}


static in_addr_t _inet_tomask (unsigned int prefix) {
  return htonl(~((1u << (32 - prefix)) - 1u));
}


static int _inet_isnetwork4 (struct in_addr network, unsigned int prefix) {
  return (network.s_addr & ~_inet_tomask(prefix)) == 0;
}


static int _inet_isnetwork6 (struct in6_addr network, unsigned int prefix) {
  int prefix32 = prefix / 32;
  int prefix32rem = prefix % 32;
  for (int i = 3; i >= prefix32 + !!prefix32rem; i--) {
    if (network.s6_addr32[i] != 0) {
      return 0;
    }
  }
  return prefix32rem == 0 ? 1 : _inet_isnetwork4(
    *(struct in_addr *) &network.s6_addr32[prefix32], prefix32rem);
}


int inet_isnetwork (int af, const void *network, unsigned int prefix) {
  switch (af) {
    case AF_INET:
      return _inet_isnetwork4(*(const struct in_addr *) network, prefix);
    case AF_INET6:
      return _inet_isnetwork6(*(const struct in6_addr *) network, prefix);
    default:
      errno = EAFNOSUPPORT;
      return 0;
  }
}


int inet_shift (int af, void *addr, int offset, unsigned int prefix) {
  unsigned short len = inet_size(af);
  return_if (len == 0) -1;
  should (prefix <= len) otherwise {
    errno = EINVAL;
    return -1;
  }

  if (af == AF_INET) {
    long long shift = (long long) offset << (32 - prefix);
    *(uint32_t *) addr = htonl(ntohl(*(uint32_t *) addr) + shift);
  } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    __int128 shift = (__int128) offset << (128 - prefix);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    union in6_addr_ {
      uint32_t addr32[4];
      __int128 addr128;
    };
    union in6_addr_ host;
    union in6_addr_ *net = (union in6_addr_ *) addr;
    host.addr32[0] = ntohl(net->addr32[3]);
    host.addr32[1] = ntohl(net->addr32[2]);
    host.addr32[2] = ntohl(net->addr32[1]);
    host.addr32[3] = ntohl(net->addr32[0]);
    host.addr128 += shift;
    net->addr32[0] = htonl(host.addr32[3]);
    net->addr32[1] = htonl(host.addr32[2]);
    net->addr32[2] = htonl(host.addr32[1]);
    net->addr32[3] = htonl(host.addr32[0]);
#elif __BYTE_ORDER == __BIG_ENDIAN
    *(__int128 *) addr += shift;
#else
  #error "Compiler did not define __BYTE_ORDER"
#endif
#pragma GCC diagnostic pop
  }
  return 0;
}
