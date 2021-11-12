#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include "macro.h"

#include "iface.h"


int tun_alloc (char ifname[IF_NAMESIZE], int flags) {
  int fd = open("/dev/net/tun", O_RDWR);
  should (fd >= 0) otherwise {
    perror("Error when opening /dev/net/tun");
    return fd;
  }

  struct ifreq ifr = {
    .ifr_flags = flags | IFF_NO_PI,
  };
  if (ifname != NULL) {
    memcpy(ifr.ifr_name, ifname, IF_NAMESIZE);
  }

  int err = ioctl(fd, TUNSETIFF, &ifr);
  should (err >= 0) otherwise {
    int saved_errno = errno;
    perror("tun_alloc: ioctl(TUNSETIFF)");
    close(fd);
    errno = saved_errno;
    return err;
  }

  if (ifname != NULL) {
    memcpy(ifname, ifr.ifr_name, IF_NAMESIZE);
  }
  return fd;
}


int tuns_alloc (
    char ifname[IF_NAMESIZE], int flags, int count, int fds[static count]) {
  return_if_fail (count > 0) 255;
  char buffer[IF_NAMESIZE];
  if (ifname == NULL) {
    ifname = buffer;
  }
  for (int i = 0; i < count; i++) {
    fds[i] = tun_alloc(ifname, flags | IFF_MULTI_QUEUE);
    should (fds[i] >= 0) otherwise {
      int saved_errno = errno;
      for (i--; i >= 0; i--) {
        close(fds[i]);
      }
      errno = saved_errno;
      return 1;
    }
  }
  return 0;
}


int ifup (const char ifname[static IF_NAMESIZE]) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  should (fd >= 0) otherwise {
    perror("ifup: socket(SOCK_DGRAM)");
    return fd;
  }

  struct ifreq ifr = {
    .ifr_flags = IFF_UP,
  };
  memcpy(ifr.ifr_name, ifname, IF_NAMESIZE);

  int err = ioctl(fd, SIOCSIFFLAGS, &ifr);
  close(fd);
  should (err >= 0) otherwise {
    perror("ifup: ioctl(SIOCSIFFLAGS)");
    return err;
  }

  return 0;
}
