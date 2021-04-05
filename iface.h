#ifndef IFACE_H
#define IFACE_H

#include <net/if.h>


///* tun_alloc: allocates or reconnects to a tun/tap device.
__attribute__((access(read_write, 1)))
int tun_alloc (char ifname[static IF_NAMESIZE], int flags);

__attribute__((nonnull, access(read_only, 1)))
int ifup (const char ifname[IF_NAMESIZE]);


#endif /* IFACE_H */
