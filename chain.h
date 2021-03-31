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
    struct FakeHost *v4_chain;
    struct FakeHost6 *v6_chain;
    char *_buf;
  };
  bool v6;
  unsigned char prefix;
  union {
    struct in_addr v4_network;
    struct in6_addr v6_network;
    char network[1];
  };
};


__attribute__((nonnull, pure, access(read_only, 1), access(read_only, 2)))
int HostChain_compare (
  const struct HostChain * restrict self, const struct HostChain * restrict other);
__attribute__((nonnull, pure, access(read_only, 1), access(read_only, 2)))
bool HostChain_in (
  const struct HostChain * restrict self, const void * restrict addr);
__attribute__((nonnull, pure, access(read_only, 1), access(read_only, 2), access(write_only, 4), access(write_only, 5)))
void *HostChain_find (
  const struct HostChain * restrict self, const void * restrict addr,
  unsigned char ttl, bool *found, unsigned char *index);
__attribute__((const))
const char *HostChain_strerror (int errnum);
__attribute__((nonnull))
void HostChain_destroy (struct HostChain * restrict self);
__attribute__((nonnull, warn_unused_result, access(read_only, 2)))
int HostChain_init (
  struct HostChain * restrict self, const char * restrict s, bool v6);


/***/

__attribute__((nonnull, pure, access(read_only, 1), access(read_only, 2), access(write_only, 4)))
void *HostChainArray_find (
  const struct HostChain * restrict self, const void * restrict addr,
  unsigned char ttl, unsigned char *index);
__attribute__((nonnull, access(read_only, 1), access(read_only, 2), access(write_only, 4), access(write_only, 5)))
int HostChain4Array_reply (
  const struct HostChain * restrict self,
  const struct iphdr * restrict receive, unsigned short receive_len,
  struct iphdr * restrict send, unsigned short * restrict send_len);
__attribute__((nonnull, access(read_only, 1), access(read_only, 2), access(write_only, 4), access(write_only, 5)))
int HostChain6Array_reply (
  const struct HostChain * restrict self,
  const struct ip6_hdr * restrict receive, unsigned short receive_len,
  struct ip6_hdr * restrict send, unsigned short * restrict send_len);
__attribute__((nonnull, pure, access(read_only, 1)))
size_t HostChainArray_nitem (const struct HostChain * restrict self);
__attribute__((nonnull))
void HostChainArray_sort (struct HostChain * restrict self);
__attribute__((nonnull))
void HostChainArray_destroy (struct HostChain * restrict self);


#endif /* CHAIN_H */
