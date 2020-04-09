#include <stddef.h>
#include <stdint.h>

#include "checksum.h"


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
  if (ip_cksum != NULL) {
    *ip_cksum = 0;
  }

  uint16_t sum = inet_cksum_finish(inet_cksum_continue(0, buf, count));

  if (ip_cksum != NULL) {
    *ip_cksum = sum;
  }
  return sum;
}
