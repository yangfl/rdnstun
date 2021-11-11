#ifndef CHAIN_H
#define CHAIN_H

#include <stdbool.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

// #include "host.h"
struct FakeHost;
struct FakeHost6;


struct HostChain {
  union {
    struct FakeHost *v4_hosts;
    struct FakeHost6 *v6_hosts;
    char *_buf;
  };
  union {
    struct in_addr v4_network;
    struct in6_addr v6_network;
    char network[1];
  };
  unsigned char prefix;
  bool v6;
};


__attribute__((nonnull, pure, warn_unused_result, access(read_only, 1)))
size_t HostChain_nitem (const struct HostChain *self);
__attribute__((nonnull, pure, warn_unused_result,
               access(read_only, 1), access(read_only, 2)))
int HostChain_compare (
  const struct HostChain * restrict self,
  const struct HostChain * restrict other);
__attribute__((nonnull, pure, warn_unused_result,
               access(read_only, 1), access(read_only, 2)))
bool HostChain_in (
  const struct HostChain * restrict self, const void * restrict addr);
__attribute__((
  nonnull, warn_unused_result, access(read_only, 1), access(read_only, 2),
  access(write_only, 4), access(write_only, 5)))
void *HostChain_find (
  const struct HostChain * restrict self, const void * restrict addr,
  unsigned char ttl, bool *found, unsigned char *index);
__attribute__((nonnull))
int HostChain_shift (
  struct HostChain *self, int offset, unsigned short prefix);
__attribute__((const, warn_unused_result))
const char *HostChain_strerror (int errnum);
__attribute__((nonnull))
void HostChain_destroy (struct HostChain *self);
__attribute__((nonnull, warn_unused_result, access(read_only, 2)))
int HostChain_copy (
  struct HostChain * restrict self, const struct HostChain * restrict other);
__attribute__((nonnull, warn_unused_result, access(read_only, 2)))
int HostChain_init (
  struct HostChain * restrict self, const char * restrict s, bool v6);


/***/

__attribute__((nonnull, warn_unused_result, access(read_only, 1),
               access(read_only, 2), access(write_only, 4)))
void *HostChainArray_find (
  const struct HostChain * restrict self, const void * restrict addr,
  unsigned char ttl, unsigned char *index);
__attribute__((nonnull, access(read_only, 1)))
int HostChain4Array_reply (
  const struct HostChain * restrict self, void *packet, unsigned short *len);
__attribute__((nonnull, access(read_only, 1)))
int HostChain6Array_reply (
  const struct HostChain * restrict self, void *packet, unsigned short *len);
__attribute__((nonnull, pure, warn_unused_result, access(read_only, 1)))
size_t HostChainArray_nitem (const struct HostChain *self);
__attribute__((nonnull))
void HostChainArray_sort (struct HostChain *self);
__attribute__((nonnull))
void HostChainArray_destroy_size (struct HostChain *self, size_t n);
__attribute__((nonnull))
void HostChainArray_destroy (struct HostChain *self);


#endif /* CHAIN_H */
