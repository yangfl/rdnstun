#ifndef HOST_H
#define HOST_H

#include <netinet/ip.h>
#include <netinet/ip6.h>


struct FakeHost {
  unsigned char ttl;
  unsigned short mtu;
  struct in_addr addr;
};

__attribute__((nonnull, pure, warn_unused_result, access(read_only, 1)))
bool BaseFakeHost_isnull (const struct FakeHost * restrict self, bool v6);
__attribute__((nonnull, access(read_only, 2)))
int BaseFakeHost_init (
  struct FakeHost * restrict self, const void * restrict addr,
  unsigned char ttl, unsigned short mtu, bool v6);
__attribute__((nonnull, access(read_only, 1)))
int FakeHost_reply (
  const struct FakeHost * restrict self, unsigned char ttl,
  void *packet, unsigned short *len);
__attribute__((nonnull, access(read_only, 2)))
int FakeHost_init (
  struct FakeHost * restrict self, const struct in_addr *addr,
  unsigned char ttl, unsigned short mtu);


/***/

struct FakeHost6 {
  unsigned char ttl;
  unsigned short mtu;
  struct in6_addr addr;
};


__attribute__((nonnull, access(read_only, 1)))
int FakeHost6_reply (
  const struct FakeHost6 * restrict self, unsigned char ttl,
  void *packet, unsigned short *len);
__attribute__((nonnull, access(read_only, 2)))
int FakeHost6_init (
  struct FakeHost6 * restrict self, const struct in6_addr *addr,
  unsigned char ttl, unsigned short mtu);


#endif /* HOST_H */
