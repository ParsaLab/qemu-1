//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
/*
 * Copyright (c) 2013
 * Guillaume Subiron
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "slirp.h"
#include "udp.h"
#include "dhcpv6.h"

void udp6_input(struct mbuf *m)
{
    Slirp *slirp = m->slirp;
    struct ip6 *ip, save_ip;
    struct udphdr *uh;
    int iphlen = sizeof(struct ip6);
    int len;
    struct socket *so;
    struct sockaddr_in6 lhost;
    struct in6_addr ip_dst;

    DEBUG_CALL("udp6_input");
    DEBUG_ARG("m = %lx", (long)m);

    if (slirp->restricted) {
        goto bad;
    }

    ip = mtod(m, struct ip6 *);
    m->m_len -= iphlen;
    m->m_data += iphlen;
    uh = mtod(m, struct udphdr *);
    m->m_len += iphlen;
    m->m_data -= iphlen;

    if (ip6_cksum(m)) {
        goto bad;
    }

    len = ntohs((uint16_t)uh->uh_ulen);

    /*
     * Make mbuf data length reflect UDP length.
     * If not enough data to reflect UDP length, drop.
     */
    if (ntohs(ip->ip_pl) != len) {
        if (len > ntohs(ip->ip_pl)) {
            goto bad;
        }
        m_adj(m, len - ntohs(ip->ip_pl));
        ip->ip_pl = htons(len);
    }

    /*
     * Save a copy of the IP header in case we want restore it
     * for sending an ICMP error message in response.
     */
    save_ip = *ip;

    /* Locate pcb for datagram. */
    lhost.sin6_family = AF_INET6;
    lhost.sin6_addr = ip->ip_src;
    lhost.sin6_port = uh->uh_sport;

    /* handle DHCPv6 */
    ip_dst = ip->ip_dst;
    if (ntohs(uh->uh_dport) == DHCPV6_SERVER_PORT &&
        (in6_equal(&ip_dst, &slirp->vhost_addr6) ||
         in6_equal(&ip_dst, &(struct in6_addr)ALLDHCP_MULTICAST))) {
        m->m_data += iphlen;
        m->m_len -= iphlen;
        dhcpv6_input(&lhost, m);
        m->m_data -= iphlen;
        m->m_len += iphlen;
        goto bad;
    }

    /* handle TFTP */
    if (ntohs(uh->uh_dport) == TFTP_SERVER &&
        !memcmp(ip->ip_dst.s6_addr, slirp->vhost_addr6.s6_addr, 16)) {
        m->m_data += iphlen;
        m->m_len -= iphlen;
        tftp_input((struct sockaddr_storage *)&lhost, m);
        m->m_data -= iphlen;
        m->m_len += iphlen;
        goto bad;
    }

    so = solookup(&slirp->udp_last_so, &slirp->udb,
                  (struct sockaddr_storage *) &lhost, NULL);

    if (so == NULL) {
        /* If there's no socket for this packet, create one. */
        so = socreate(slirp);
        if (!so) {
            goto bad;
        }
        if (udp_attach(so, AF_INET6) == -1) {
            DEBUG_MISC((dfd, " udp6_attach errno = %d-%s\n",
                        errno, strerror(errno)));
            sofree(so);
            goto bad;
        }

        /* Setup fields */
        so->so_lfamily = AF_INET6;
        so->so_laddr6 = ip->ip_src;
        so->so_lport6 = uh->uh_sport;
    }

    so->so_ffamily = AF_INET6;
    so->so_faddr6 = ip->ip_dst; /* XXX */
    so->so_fport6 = uh->uh_dport; /* XXX */

    iphlen += sizeof(struct udphdr);
    m->m_len -= iphlen;
    m->m_data += iphlen;

    /*
     * Now we sendto() the packet.
     */
    if (sosendto(so, m) == -1) {
        m->m_len += iphlen;
        m->m_data -= iphlen;
        *ip = save_ip;
        DEBUG_MISC((dfd, "udp tx errno = %d-%s\n", errno, strerror(errno)));
        icmp6_send_error(m, ICMP6_UNREACH, ICMP6_UNREACH_NO_ROUTE);
        goto bad;
    }

    m_free(so->so_m);   /* used for ICMP if error on sorecvfrom */

    /* restore the orig mbuf packet */
    m->m_len += iphlen;
    m->m_data -= iphlen;
    *ip = save_ip;
    so->so_m = m;

    return;
bad:
    m_free(m);
}

int udp6_output(struct socket *so, struct mbuf *m,
        struct sockaddr_in6 *saddr, struct sockaddr_in6 *daddr)
{
    struct ip6 *ip;
    struct udphdr *uh;

    DEBUG_CALL("udp6_output");
    DEBUG_ARG("so = %lx", (long)so);
    DEBUG_ARG("m = %lx", (long)m);

    /* adjust for header */
    m->m_data -= sizeof(struct udphdr);
    m->m_len += sizeof(struct udphdr);
    uh = mtod(m, struct udphdr *);
    m->m_data -= sizeof(struct ip6);
    m->m_len += sizeof(struct ip6);
    ip = mtod(m, struct ip6 *);

    /* Build IP header */
    ip->ip_pl = htons(m->m_len - sizeof(struct ip6));
    ip->ip_nh = IPPROTO_UDP;
    ip->ip_src = saddr->sin6_addr;
    ip->ip_dst = daddr->sin6_addr;

    /* Build UDP header */
    uh->uh_sport = saddr->sin6_port;
    uh->uh_dport = daddr->sin6_port;
    uh->uh_ulen = ip->ip_pl;
    uh->uh_sum = 0;
    uh->uh_sum = ip6_cksum(m);
    if (uh->uh_sum == 0) {
        uh->uh_sum = 0xffff;
    }

    return ip6_output(so, m, 0);
}
