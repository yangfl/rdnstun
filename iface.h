#ifndef IFACE_H
#define IFACE_H

#include <net/if.h>


///* tun_alloc: allocates or reconnects to a tun/tap device.
__attribute__((access(read_write, 1)))
int tun_alloc (char ifname[IF_NAMESIZE], int flags);
__attribute__((nonnull(4), access(read_write, 1), access(write_only, 4, 3)))
int tuns_alloc (
  char ifname[IF_NAMESIZE], int flags, int count, int fds[static count]);
__attribute__((nonnull, access(read_only, 1)))
int ifup (const char ifname[static IF_NAMESIZE]);


#endif /* IFACE_H */
