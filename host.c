#include <endian.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>

#include "macro.h"
#include "inet.h"
#include "log.h"
#include "rdnstun.h"
#include "host.h"


bool BaseFakeHost_isnull (const struct FakeHost * restrict self, bool v6) {
  return v6 ?
    IN6_IS_ADDR_UNSPECIFIED(&self->addr) : self->addr.s_addr == INADDR_ANY;
}


int BaseFakeHost_init (
    struct FakeHost * restrict self, const void * restrict addr,
    unsigned char ttl, unsigned short mtu, bool v6) {
  self->ttl = ttl;
  self->mtu = mtu;
  memcpy(&self->addr, addr,
         v6 ? sizeof(struct in6_addr) : sizeof(struct in_addr));
  return 0;
}


int FakeHost_reply (
    const struct FakeHost * restrict self, unsigned char ttl,
    void *packet, unsigned short *len) {
  struct ipicmp {
    struct ip ip;
    union {
      char data[8];
      struct {
        struct icmphdr icmp;
        struct ip orig_ip;
        char orig_data[8];
      };
    };
  } *pkt = packet;

  return_if_fail (pkt->ip.ip_ttl > ttl) 1;

  // find reply host
  unsigned char receive_ttl = pkt->ip.ip_ttl - ttl;
  bool dst_is_target = self->addr.s_addr == pkt->ip.ip_dst.s_addr;

  if (LogLevel_should_log(LOG_LEVEL_DEBUG)) {
    char s_src_addr[INET_ADDRSTRLEN];
    char s_dst_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pkt->ip.ip_src, s_src_addr, sizeof(s_src_addr));
    inet_ntop(AF_INET, &pkt->ip.ip_dst, s_dst_addr, sizeof(s_dst_addr));
    if (dst_is_target) {
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%d)",
             s_src_addr, pkt->ip.ip_ttl, s_dst_addr, receive_ttl,
             pkt->ip.ip_ttl);
    } else {
      char s_self_addr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &pkt->ip.ip_src, s_self_addr, sizeof(s_self_addr));
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%s %d)",
             s_src_addr, pkt->ip.ip_ttl, s_dst_addr, receive_ttl,
             s_self_addr, pkt->ip.ip_ttl);
    }
  }

  // prepare reply
  uint8_t type;
  uint8_t code;
  if (receive_ttl == 1 && !dst_is_target) {
    // not us and ttl exceeded
    type = ICMP_TIMXCEED;
    code = ICMP_TIMXCEED_INTRANS;
  } else if (!dst_is_target) {
    // not us and ttl not exceeded (at the end of chain), wrong route
    type = ICMP_UNREACH;
    code = ICMP_UNREACH_HOST;
  } else if (pkt->ip.ip_p == IPPROTO_ICMP) {
    if (pkt->icmp.type != ICMP_ECHO) {
      // ignore non-ping
      *len = 0;
      return 0;
    }
    // ping
    pkt->icmp.type = ICMP_ECHOREPLY;
    pkt->icmp.code = 0;
    if (*len <= self->mtu) {
      uint32_t checksum = pkt->icmp.checksum;
      checksum += le16toh(ICMP_ECHO) - le16toh(ICMP_ECHOREPLY);
      pkt->icmp.checksum = (checksum >> 16) + (checksum & 0xffff);
    } else {
      *len = self->mtu;
      pkt->ip.ip_len = htons(*len);
      inet_cksum(&pkt->icmp.checksum, &pkt->icmp, *len - sizeof(struct ip));
    }
    goto no_append;
  } else {
    // other packet, reply port closed
    type = ICMP_UNREACH;
    code = ICMP_UNREACH_PORT;
  }

  // append new header
  *len = sizeof(struct ipicmp);
  pkt->orig_ip = pkt->ip;
  memcpy(pkt->orig_data, pkt->data, sizeof(pkt->orig_data));

  // fix icmp header
  pkt->icmp.type = type;
  pkt->icmp.code = code;
  memset(&pkt->icmp.un, 0, sizeof(pkt->icmp.un));
  inet_cksum(&pkt->icmp.checksum, &pkt->icmp,
             sizeof(struct ipicmp) - sizeof(struct ip));

  // fix ip header
  pkt->ip.ip_p = IPPROTO_ICMP;
  pkt->ip.ip_len = htons(sizeof(struct ipicmp));
no_append:
  pkt->ip.ip_ttl = self->ttl - ttl;
  pkt->ip.ip_dst = pkt->ip.ip_src;
  pkt->ip.ip_src = self->addr;
  inet_cksum_header(&pkt->ip.ip_sum, pkt, sizeof(struct ip));

  return 0;
}


int FakeHost_init (
    struct FakeHost * restrict self, const struct in_addr *addr,
    unsigned char ttl, unsigned short mtu) {
  return BaseFakeHost_init(self, addr, ttl, mtu, false);
}


int FakeHost6_reply (
    const struct FakeHost6 * restrict self, unsigned char ttl,
    void *packet, unsigned short *len) {
  struct ipicmp6 {
    struct ip6_hdr ip;
    union {
      char data[8];
      struct {
        struct icmp6_hdr icmp;
        struct ip6_hdr orig_ip;
        char orig_data[8];
      };
    };
  } *pkt = packet;

  return_if_fail (pkt->ip.ip6_hlim > ttl) 1;

  // find reply host
  unsigned char receive_ttl = pkt->ip.ip6_hlim - ttl;
  bool dst_is_target = IN6_ARE_ADDR_EQUAL(&self->addr, &pkt->ip.ip6_dst);

  if (LogLevel_should_log(LOG_LEVEL_DEBUG)) {
    char s_src_addr[INET6_ADDRSTRLEN];
    char s_dst_addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &pkt->ip.ip6_src, s_src_addr, sizeof(s_src_addr));
    inet_ntop(AF_INET6, &pkt->ip.ip6_dst, s_dst_addr, sizeof(s_dst_addr));
    if (dst_is_target) {
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%d)",
             s_src_addr, pkt->ip.ip6_hlim, s_dst_addr, receive_ttl,
             pkt->ip.ip6_hlim);
    } else {
      char s_self_addr[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &pkt->ip.ip6_src, s_self_addr, sizeof(s_self_addr));
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%s %d)",
             s_src_addr, pkt->ip.ip6_hlim, s_dst_addr, receive_ttl,
             s_self_addr, pkt->ip.ip6_hlim);
    }
  }

  // prepare reply
  uint8_t type;
  uint8_t code;
  bool checksum_ok = false;
  if (receive_ttl == 1 && !dst_is_target) {
    // not us and ttl exceeded
    type = ICMP6_TIME_EXCEEDED;
    code = ICMP6_TIME_EXCEED_TRANSIT;
  } else if (!dst_is_target) {
    // not us and ttl not exceeded (at the end of chain), wrong route
    type = ICMP6_DST_UNREACH;
    code = ICMP6_DST_UNREACH_ADDR;
  } else if (pkt->ip.ip6_nxt == IPPROTO_ICMPV6) {
    if (pkt->icmp.icmp6_type != ICMP6_ECHO_REQUEST) {
      // ignore non-ping
      *len = 0;
      return 0;
    }
    // ping
    pkt->icmp.icmp6_type = ICMP6_ECHO_REPLY;
    pkt->icmp.icmp6_code = 0;
    if (*len <= self->mtu) {
      uint32_t checksum = pkt->icmp.icmp6_cksum;
      checksum += le16toh(ICMP6_ECHO_REQUEST) - le16toh(ICMP6_ECHO_REPLY);
      pkt->icmp.icmp6_cksum = (checksum >> 16) + (checksum & 0xffff);
      checksum_ok = true;
    } else {
      *len = self->mtu;
      pkt->ip.ip6_plen = htons(*len - sizeof(struct ip6_hdr));
    }
    goto no_append;
  } else {
    // other packet, reply port closed
    type = ICMP6_DST_UNREACH;
    code = ICMP6_DST_UNREACH_NOPORT;
  }

  // append new header
  *len = sizeof(struct ipicmp6);
  pkt->orig_ip = pkt->ip;
  memcpy(pkt->orig_data, pkt->data, sizeof(pkt->orig_data));

  // fix icmp header
  pkt->icmp.icmp6_type = type;
  pkt->icmp.icmp6_code = code;
  memset(&pkt->icmp.icmp6_dataun, 0, sizeof(pkt->icmp.icmp6_dataun));

  // fix ip header
  pkt->ip.ip6_plen = htons(sizeof(struct ipicmp6) - sizeof(struct ip6_hdr));
  pkt->ip.ip6_nxt = IPPROTO_ICMPV6;
no_append:
  pkt->ip.ip6_hlim = self->ttl - ttl;
  pkt->ip.ip6_dst = pkt->ip.ip6_src;
  pkt->ip.ip6_src = self->addr;

  if (!checksum_ok) {
    // fill icmp header checksum
    pkt->icmp.icmp6_cksum = 0;
    uint32_t sum = inet_cksum_continue(0, &pkt->ip.ip6_src, 32);
    sum += pkt->ip.ip6_plen;
    sum += htons(pkt->ip.ip6_nxt);
    sum = inet_cksum_continue(sum, &pkt->icmp, *len - sizeof(struct ip6_hdr));
    pkt->icmp.icmp6_cksum = inet_cksum_finish(sum);
  }

  return 0;
}


int FakeHost6_init (
    struct FakeHost6 * restrict self, const struct in6_addr *addr,
    unsigned char ttl, unsigned short mtu) {
  return BaseFakeHost_init((struct FakeHost *) self, addr, ttl, mtu, true);
}
