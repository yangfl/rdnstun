#ifndef IFACE_H
#define IFACE_H

#include <net/if.h>


///* tun_alloc: allocates or reconnects to a tun/tap device.
int tun_alloc (char ifname[IFNAMSIZ], int flags);

__attribute__((nonnull, access(read_only, 1)))
int ifup (const char ifname[IFNAMSIZ]);


#endif /* IFACE_H */
