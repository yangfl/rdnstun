#ifndef INET_H
#define INET_H

#include <stddef.h>
#include <stdint.h>


inline uint16_t inet_cksum_finish (uint32_t sum) {
  // Fold 32-bit sum to 16 bits
  sum = (sum >> 16) + (sum & 0xffff);  /* add high-16 to low-16 */
  sum += (sum >> 16);  /* add carry */
  sum = ~sum;
  return sum;
}

__attribute__((nonnull, access(read_only, 2, 3)))
uint32_t inet_cksum_continue (uint32_t sum, const void *buf, size_t count);
__attribute__((nonnull, access(read_only, 2, 3)))
uint16_t inet_cksum_header (void *cksum, const void *buf, size_t count);
__attribute__((nonnull(2), access(read_only, 2, 3)))
uint16_t inet_cksum (void *cksum, const void *buf, size_t count);
__attribute__((nonnull, access(read_only, 1), access(read_only, 2)))
int membcmp (const void *s1, const void *s2, size_t n);
__attribute__((nonnull, pure, warn_unused_result, access(read_only, 2)))
int inet_isnetwork (int af, const void *network, unsigned int prefix);
__attribute__((nonnull))
int inet_shift (int af, void *addr, int offset, unsigned int prefix);


#endif /* INET_H */
