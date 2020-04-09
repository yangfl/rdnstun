#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stddef.h>
#include <stdint.h>


uint32_t inet_cksum_continue (uint32_t sum, const void *buf, size_t count);
uint16_t inet_cksum_finish (uint32_t sum);
uint16_t inet_cksum (void *cksum, const void *buf, size_t count);


#endif /* CHECKSUM_H */
