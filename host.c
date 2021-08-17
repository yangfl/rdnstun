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
    const struct FakeHost * restrict self,
    const struct iphdr * restrict receive, unsigned short receive_len,
    unsigned char ttl,
    struct iphdr * restrict send, unsigned short * restrict send_len) {
  return_if_fail (receive->ttl > ttl) 1;

  // find reply host
  unsigned char receive_ttl = receive->ttl - ttl;
  bool dst_is_target = self->addr.s_addr == receive->daddr;

  // prepare ip header
  memcpy(send, receive, sizeof(struct ip));
  send->ttl = self->ttl - ttl;
  send->protocol = IPPROTO_ICMP;
  send->saddr = self->addr.s_addr;
  send->daddr = receive->saddr;

  if (LogLevel_should_log(LOG_LEVEL_DEBUG)) {
    char s_src_addr[INET_ADDRSTRLEN];
    char s_dst_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &receive->saddr, s_src_addr, sizeof(s_src_addr));
    inet_ntop(AF_INET, &receive->daddr, s_dst_addr, sizeof(s_dst_addr));
    if (dst_is_target) {
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%d)",
             s_src_addr, receive->ttl, s_dst_addr, receive_ttl, send->ttl);
    } else {
      char s_self_addr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &send->saddr, s_self_addr, sizeof(s_self_addr));
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%s %d)",
             s_src_addr, receive->ttl, s_dst_addr, receive_ttl,
             s_self_addr, send->ttl);
    }
  }

  // prepare reply
  const struct icmp *receive_icmp = (struct icmp *) (receive + 1);
  struct icmp *send_icmp = (struct icmp *) (send + 1);
  send_icmp->icmp_void = 0;
  if (receive_ttl == 1 && !dst_is_target) {
    // not us and ttl exceeded
    send_icmp->icmp_type = ICMP_TIMXCEED;
    send_icmp->icmp_code = ICMP_TIMXCEED_INTRANS;
  } else if (!dst_is_target) {
    // not us and ttl not exceeded (at the end of chain), wrong route
    send_icmp->icmp_type = ICMP_UNREACH;
    send_icmp->icmp_code = ICMP_UNREACH_HOST;
  } else if (receive->protocol == IPPROTO_ICMP &&
             receive_icmp->icmp_type == ICMP_ECHO) {
    // ping
    send_icmp->icmp_type = ICMP_ECHOREPLY;
    send_icmp->icmp_code = 0;
    send_icmp->icmp_id = receive_icmp->icmp_id;
    send_icmp->icmp_seq = receive_icmp->icmp_seq;
    *send_len = receive_len - sizeof(struct ip) - ICMP_MINLEN;
    memcpy(send_icmp->icmp_data, receive_icmp->icmp_data, *send_len);
    goto v4_no_copy_received_pkt;
  } else {
    // other packet, reply port closed
    send_icmp->icmp_type = ICMP_UNREACH;
    send_icmp->icmp_code = ICMP_UNREACH_PORT;
  }
  *send_len = min(receive_len, self->mtu - sizeof(struct ip) - ICMP_MINLEN);
  memcpy(send_icmp->icmp_data, receive, *send_len);
v4_no_copy_received_pkt:

  // fill icmp header checksum
  *send_len += ICMP_MINLEN;
  inet_cksum(&send_icmp->icmp_cksum, send_icmp, *send_len);

  // fill ip header checksum
  *send_len += sizeof(struct ip);
  send->tot_len = htons(*send_len);
  inet_cksum(&send->check, send, *send_len);

  return 0;
}


int FakeHost_init (
    struct FakeHost * restrict self, const struct in_addr *addr,
    unsigned char ttl, unsigned short mtu) {
  return BaseFakeHost_init(self, addr, ttl, mtu, false);
}


int FakeHost6_reply (
    const struct FakeHost6 * restrict self,
    const struct ip6_hdr * restrict receive, unsigned short receive_len,
    unsigned char ttl,
    struct ip6_hdr * restrict send, unsigned short * restrict send_len) {
  return_if_fail (receive->ip6_hlim > ttl) 1;

  // find reply host
  unsigned char receive_ttl = receive->ip6_hlim - ttl;
  bool dst_is_target = IN6_ARE_ADDR_EQUAL(&self->addr, &receive->ip6_dst);

  // prepare send ip header
  memcpy(send, receive, sizeof(struct ip6_hdr));
  send->ip6_hlim = self->ttl - ttl;
  send->ip6_nxt = IPPROTO_ICMPV6;
  memcpy(&send->ip6_src, &self->addr, sizeof(struct in6_addr));
  memcpy(&send->ip6_dst, &receive->ip6_src, sizeof(struct in6_addr));

  if (LogLevel_should_log(LOG_LEVEL_DEBUG)) {
    char s_src_addr[INET6_ADDRSTRLEN];
    char s_dst_addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &receive->ip6_src, s_src_addr, sizeof(s_src_addr));
    inet_ntop(AF_INET6, &receive->ip6_dst, s_dst_addr, sizeof(s_dst_addr));
    if (dst_is_target) {
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%d)",
             s_src_addr, receive->ip6_hlim, s_dst_addr, receive_ttl,
             send->ip6_hlim);
    } else {
      char s_self_addr[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &send->ip6_src, s_self_addr, sizeof(s_self_addr));
      LOGGER(RDNSTUN_NAME, LOG_LEVEL_DEBUG, "%s %d -> %s %d (%s %d)",
             s_src_addr, receive->ip6_hlim, s_dst_addr, receive_ttl,
             s_self_addr, send->ip6_hlim);
    }
  }

  // prepare reply
  const struct icmp6_hdr *receive_icmp =
    (struct icmp6_hdr *) (receive + 1);
  struct icmp6_hdr *send_icmp =
    (struct icmp6_hdr *) (send + 1);
  *send_icmp->icmp6_data32 = 0;
  if (receive_ttl == 1 && !dst_is_target) {
    // not us and ttl exceeded
    send_icmp->icmp6_type = ICMP6_TIME_EXCEEDED;
    send_icmp->icmp6_code = ICMP6_TIME_EXCEED_TRANSIT;
  } else if (!dst_is_target) {
    // not us and ttl not exceeded (at the end of chain), wrong route
    send_icmp->icmp6_type = ICMP6_DST_UNREACH;
    send_icmp->icmp6_code = ICMP6_DST_UNREACH_ADDR;
  } else if (receive->ip6_nxt == IPPROTO_ICMPV6 &&
             receive_icmp->icmp6_type == ICMP6_ECHO_REQUEST) {
    // ping
    send_icmp->icmp6_type = ICMP6_ECHO_REPLY;
    send_icmp->icmp6_code = 0;
    *send_icmp->icmp6_data32 = *receive_icmp->icmp6_data32;
    *send_len = receive_len - sizeof(struct ip6_hdr) - sizeof(struct icmp6_hdr);
    memcpy(send_icmp + 1, receive_icmp + 1, *send_len);
    goto v6_no_copy_received_pkt;
  } else {
    // other packet, reply port closed
    send_icmp->icmp6_type = ICMP6_DST_UNREACH;
    send_icmp->icmp6_code = ICMP6_DST_UNREACH_NOPORT;
  }
  *send_len = min(
    receive_len, self->mtu - sizeof(struct ip6_hdr) - sizeof(struct icmp6_hdr));
  memcpy(send_icmp + 1, receive, *send_len);
v6_no_copy_received_pkt:

  // fill icmp header checksum
  *send_len += sizeof(struct icmp6_hdr);
  send_icmp->icmp6_cksum = 0;
  uint32_t sum = inet_cksum_continue(0, &send->ip6_src, 32);
  sum += htonl(*send_len);
  sum += htonl(send->ip6_nxt);
  sum = inet_cksum_continue(sum, send_icmp, *send_len);
  send_icmp->icmp6_cksum = inet_cksum_finish(sum);

  // fill ip header checksum
  send->ip6_plen = htons(*send_len);
  *send_len += sizeof(struct ip6_hdr);

  return 0;
}


int FakeHost6_init (
    struct FakeHost6 * restrict self, const struct in6_addr *addr,
    unsigned char ttl, unsigned short mtu) {
  return BaseFakeHost_init((struct FakeHost *) self, addr, ttl, mtu, true);
}
