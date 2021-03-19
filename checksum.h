#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stddef.h>
#include <stdint.h>


__attribute__((nonnull, access(read_only, 2, 3)))
uint32_t inet_cksum_continue (uint32_t sum, const void *buf, size_t count);
uint16_t inet_cksum_finish (uint32_t sum);
__attribute__((nonnull(2), access(read_only, 2, 3)))
uint16_t inet_cksum (void *cksum, const void *buf, size_t count);


#endif /* CHECKSUM_H */
