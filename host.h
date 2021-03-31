#ifndef HOST_H
#define HOST_H

#include <netinet/ip.h>
#include <netinet/ip6.h>


struct FakeHost {
  unsigned char ttl;
  unsigned short mtu;
  struct in_addr addr;
};

__attribute__((nonnull, pure, access(read_only, 1)))
bool BaseFakeHost_isnull (const struct FakeHost * restrict self, bool v6);
__attribute__((nonnull, access(read_only, 2)))
int BaseFakeHost_init (
  struct FakeHost * restrict self, const void * restrict addr,
  unsigned char ttl, unsigned short mtu, bool v6);
__attribute__((nonnull, access(read_only, 1), access(read_only, 2), access(write_only, 5), access(write_only, 6)))
int FakeHost_reply (
  const struct FakeHost * restrict self, const struct iphdr * restrict receive,
  unsigned short receive_len, unsigned char ttl, struct iphdr * restrict send,
  unsigned short * restrict send_len);
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


__attribute__((nonnull, access(read_only, 1), access(read_only, 2), access(write_only, 5), access(write_only, 6)))
int FakeHost6_reply (
  const struct FakeHost6 * restrict self,
  const struct ip6_hdr * restrict receive,
  unsigned short receive_len, unsigned char ttl,
  struct ip6_hdr * restrict send, unsigned short * restrict send_len);
__attribute__((nonnull, access(read_only, 2)))
int FakeHost6_init (
  struct FakeHost6 * restrict self, const struct in6_addr *addr,
  unsigned char ttl, unsigned short mtu);


#endif /* HOST_H */
