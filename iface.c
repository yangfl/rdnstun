#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include "macro.h"

#include "iface.h"


int tun_alloc (char ifname[IFNAMSIZ], int flags) {
  int fd = open("/dev/net/tun", O_RDWR);
  should (fd >= 0) otherwise {
    perror("Error when opening /dev/net/tun");
    return fd;
  }

  struct ifreq ifr = {
    .ifr_flags = flags | IFF_NO_PI,
  };
  if (ifname != NULL) {
    memcpy(ifr.ifr_name, ifname, IFNAMSIZ);
  }

  int err = ioctl(fd, TUNSETIFF, &ifr);
  should (err >= 0) otherwise {
    perror("tun_alloc: ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  if (ifname != NULL) {
    memcpy(ifname, ifr.ifr_name, IFNAMSIZ);
  }
  return fd;
}


int ifup (const char ifname[IFNAMSIZ]) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  should (fd >= 0) otherwise {
    perror("ifup: socket(SOCK_DGRAM)");
    return fd;
  }

  struct ifreq ifr = {
    .ifr_flags = IFF_UP,
  };
  memcpy(ifr.ifr_name, ifname, IFNAMSIZ);

  int err = ioctl(fd, SIOCSIFFLAGS, &ifr);
  close(fd);
  should (err >= 0) otherwise {
    perror("ifup: ioctl(SIOCSIFFLAGS)");
    return err;
  }

  return 0;
}
