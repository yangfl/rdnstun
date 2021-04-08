#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

#include "macro.h"
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


int inet_test_cidr (int af, const void *network, size_t prefix) {
  unsigned short len;
  switch (af) {
    case AF_INET6:
      len = 128;
      break;
    case AF_INET:
      len = 32;
      break;
    default:
      errno = EAFNOSUPPORT;
      return -1;
  }
  should (prefix <= len) otherwise {
    errno = EINVAL;
    return -1;
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
