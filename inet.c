#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

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


uint16_t inet_cksum_finish (uint32_t sum) {
  // Fold 32-bit sum to 16 bits
  sum = (sum >> 16) + (sum & 0xffff);  /* add high-16 to low-16 */
  sum += (sum >> 16);      /* add carry */
  sum = ~sum;
  return sum;
}


uint16_t inet_cksum (void *cksum, const void *buf, size_t count) {
  uint16_t *ip_cksum = cksum;
  // first, set cksum to 0
  if (ip_cksum != NULL) {
    *ip_cksum = 0;
  }
  // calculate cksum
  uint16_t sum = inet_cksum_finish(inet_cksum_continue(0, buf, count));
  // write cksum
  if (ip_cksum != NULL) {
    *ip_cksum = sum;
  }
  return sum;
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

int inet_check_cidr (int af, const void *network, unsigned short prefix) {
  unsigned short len = inet_size(af);
  return_if (len == 0) -1;
  should (prefix <= len) otherwise {
    errno = EINVAL;
    return 0;
  }
  unsigned int bytes = prefix / 8;
  unsigned int bits = prefix % 8;

  if (bits != 0) {
    if ((((const uint8_t *) network)[bytes] & ~((uint8_t) -1 << (8 - bits)))
        != 0) {
      return 0;
    }
    bytes++;
  }

  static const uint8_t mask[16] = {0};
  return memcmp((const uint8_t *) network + bytes, mask, len / 8 - bytes) == 0;
}


int inet_shift (int af, void *addr, int offset, unsigned short prefix) {
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
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    struct in6_addr_ {
      union {
        uint32_t __u6_addr32[4];
        __int128 __u6_addr128;
      };
    };
    struct in6_addr_ host;
    struct in6_addr_ *net = (struct in6_addr_ *) addr;
    host.__u6_addr32[0] = ntohl(net->__u6_addr32[3]);
    host.__u6_addr32[1] = ntohl(net->__u6_addr32[2]);
    host.__u6_addr32[2] = ntohl(net->__u6_addr32[1]);
    host.__u6_addr32[3] = ntohl(net->__u6_addr32[0]);
    host.__u6_addr128 += shift;
    net->__u6_addr32[0] = htonl(host.__u6_addr32[3]);
    net->__u6_addr32[1] = htonl(host.__u6_addr32[2]);
    net->__u6_addr32[2] = htonl(host.__u6_addr32[1]);
    net->__u6_addr32[3] = htonl(host.__u6_addr32[0]);
#else
    *(__int128 *) addr += shift;
#endif
#pragma GCC diagnostic pop
  }
  return 0;
}
