/*!
 * @brief This is the sstp-client code
 *
 * @file sstp-route.c
 *
 * @author Copyright (C) 2011 Eivind Naess, 
 *      All Rights Reserved
 *
 * @par License:
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sstp-private.h"

#ifdef HAVE_NETLINK
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif



/* Get the end of the netlink message */
#define NLMSG_TAIL(n) \
    (((void*)(n)) + NLMSG_ALIGN((n)->nlmsg_len))


/*! 
 * @brief The route context structure
 */
struct sstp_route_ctx
{
    /* The netlink socket */
    int sock;

    /* The sequence number */
    int seq;

    /* The current length of the message */
    int len;

    /* The buffer allocated to hold the message */
    char buf[1024];
};

#ifdef HAVE_NETLINK

/*!
 * @brief Receive a netlink message
 */
static int sstp_route_recv(sstp_route_ctx_st *ctx)
{
    struct nlmsghdr *nlh = (struct nlmsghdr*) ctx->buf;
    int len = 0;

    ctx->len = 0;

    do
    {
        len = recv(ctx->sock, ctx->buf + ctx->len, 
                sizeof(ctx->buf) - ctx->len, 0);
        if (len == -1)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            return -1;
        }
        if (len == 0)
        {
            return -1;
        }

        if (!NLMSG_OK(nlh, len))
        {
            printf("Could not receive data: %d %m\n", len);
            return -1;
        }
        
        if (nlh->nlmsg_type == NLMSG_ERROR)
        {
            struct nlmsgerr *err = (struct nlmsgerr*) 
                    NLMSG_DATA(nlh);
            if (err->error)
            {
                errno = -err->error;
                return -1;
            }
        }

        if (nlh->nlmsg_seq != ctx->seq)
        {
            continue;
        }

        if (nlh->nlmsg_pid != getpid())
        {
            continue;
        }

        ctx->len = len;
        if (nlh->nlmsg_type == NLMSG_DONE)
        {
            break;
        }

        if (!(nlh->nlmsg_flags & NLM_F_MULTI))
        {
            break;
        }

    } while (nlh->nlmsg_len > ctx->len);

    return ctx->len;
}


/*!
 * @brief Send a netlink message and wait for a response or ACK
 */
static int sstp_route_talk(sstp_route_ctx_st *ctx, void *data, int size)
{
    int len = -1;

    /* Send the request */
    len = send(ctx->sock, data, size, 0);
    if (len != size)
    {
        goto done;
    }

    /* Receive the response */
    len = sstp_route_recv(ctx);
    if (len < 0)
    {
        goto done;
    }

done:

    return len;
}


/*! 
 * @brief Add an attribute to the netlink message
 */
static void sstp_route_addattr(struct nlmsghdr *nlh, int type, 
        int len, void *value)
{
    struct rtattr *rta = NULL;
    
    rta = (struct rtattr*) NLMSG_TAIL(nlh);
    rta->rta_type  = type,
    rta->rta_len   = RTA_LENGTH(len);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) +
                     RTA_SPACE(len);
    memcpy(RTA_DATA(rta), value, len);
}


/*!
 * @brief Create a new netlink message
 */
static int sstp_route_newmsg(sstp_route_ctx_st *ctx, 
        sstp_route_st *route, int cmd, int flags)
{
    struct nlmsghdr *nlh;
    struct rtmsg *rtm;

    memset(ctx->buf, 0, sizeof(ctx->buf));

    /* Setup the netlink header */
    nlh = (struct nlmsghdr*) ctx->buf;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtmsg));
    nlh->nlmsg_type  = cmd;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | flags;
    nlh->nlmsg_seq   = ++ctx->seq;
    nlh->nlmsg_pid   = getpid();

    /* Setup the netlink route message */
    rtm = (struct rtmsg*) NLMSG_DATA(nlh);
    rtm->rtm_table  = RT_TABLE_MAIN;
    rtm->rtm_family = route->family;
    rtm->rtm_scope  = (route->have.gwy) 
            ? RT_SCOPE_UNIVERSE
            : RT_SCOPE_LINK ;
    if (cmd != RTM_DELROUTE)
    {
        rtm->rtm_protocol = RTPROT_BOOT;
        rtm->rtm_type     = RTN_UNICAST;
    }

    /* Add destination */
    if (route->have.dst)
    {
        sstp_route_addattr(nlh, RTA_DST, route->rt_blen, 
                &route->dst);
        rtm->rtm_dst_len = route->rt_blen << 3;
    }

    /* Add source */
    if (route->have.src)
    {
        sstp_route_addattr(nlh, RTA_PREFSRC, route->rt_blen,
                &route->src);
        rtm->rtm_src_len = route->rt_blen << 3;
    }
        
    /* Add gateway */
    if (route->have.gwy)
    {
        sstp_route_addattr(nlh, RTA_GATEWAY, route->rt_blen,
                &route->gwy);
    }

    /* Add output interface */
    if (route->have.oif)
    {
        sstp_route_addattr(nlh, RTA_OIF, sizeof(route->oif), 
                &route->oif);
    }

    return nlh->nlmsg_len;
}


/*!
 * @brief Create or replace an existing route to a destination
 */
int sstp_route_replace(sstp_route_ctx_st *ctx, sstp_route_st *route)
{
    int len = sstp_route_newmsg(ctx, route, RTM_NEWROUTE, 
            NLM_F_CREATE | NLM_F_REPLACE);
    if (len < 0)
    {
        return -1;
    }

    len = sstp_route_talk(ctx, ctx->buf, len);
    if (len < 0)
    {
        return -1;
    }

    return 0;
}


/*!
 * @brief Delete a route from the route table
 */
int sstp_route_delete(sstp_route_ctx_st *ctx, sstp_route_st *route)
{
    int len = sstp_route_newmsg(ctx, route, RTM_DELROUTE, 0);
    if (len < 0)
    {
        return -1;
    }

    len = sstp_route_talk(ctx, ctx->buf, len);
    if (len < 0)
    {
        return -1;
    }

    return 0;
}


/*!
 * @brief Lookup a particular route to a destination
 */
int sstp_route_get(sstp_route_ctx_st *ctx, struct sockaddr *dst,
        sstp_route_st *route)
{
    struct nlmsghdr *nlh;
    struct rtmsg *rtm;
    struct rtattr *rta;
    int retval = -1;
    int len = 0;
    int rtl = 0;

    memset(ctx->buf, 0, sizeof(ctx->buf));

    /* Setup the netlink header */
    nlh = (struct nlmsghdr*) ctx->buf;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtmsg));
    nlh->nlmsg_type  = RTM_GETROUTE;
    nlh->nlmsg_flags = NLM_F_REQUEST;
    nlh->nlmsg_seq   = ++ctx->seq;
    nlh->nlmsg_pid   = getpid();

    /* Setup the netlink route message */
    rtm = (struct rtmsg*) NLMSG_DATA(nlh);
    rtm->rtm_family = dst->sa_family;
    rtm->rtm_table  = RT_TABLE_MAIN;

    /* Handle the RTA per address family */
    switch (dst->sa_family)
    {
    case AF_INET:
    {
        struct sockaddr_in *in = (struct sockaddr_in*) dst;
        sstp_route_addattr(nlh, RTA_DST, sizeof(struct in_addr),
                &in->sin_addr);
        rtm->rtm_dst_len = 32;
        break;
    }
    case AF_INET6:
    {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6*) dst;
        sstp_route_addattr(nlh, RTA_DST, sizeof(struct in6_addr),
                &in6->sin6_addr);
        rtm->rtm_dst_len = 128;
        break;
    }
    }

    /* Do the route exchange */
    len = sstp_route_talk(ctx, nlh, nlh->nlmsg_len);
    if (len < 0)
    {
        goto done;
    }

    /* Check the response */
    if (!NLMSG_OK(nlh, len))
    {
        goto done;
    }

    /* Obtain the pointers to rtm, and rta */
    rtm = (struct rtmsg*)  NLMSG_DATA(nlh);
    rta = (struct rtattr*) RTM_RTA(rtm);
    rtl = RTM_PAYLOAD(nlh);

    if (rtm->rtm_family != AF_INET && rtm->rtm_family != AF_INET6)
    {
        goto done;
    }

    /* Prepare the output */
    memset(route, 0, sizeof(sstp_route_st));
    route->family = rtm->rtm_family;
    route->rt_blen= (rtm->rtm_family == AF_INET6) ? 16 : 4;

    /* Iterate over the attributes */
    for ( ; RTA_OK(rta,rtl); rta = RTA_NEXT(rta,rtl))
    {
        switch (rta->rta_type)
        {
        case RTA_OIF:
            if_indextoname(*(int*)RTA_DATA(rta), 
                    route->ifname);
            memcpy(&route->oif, RTA_DATA(rta), 
                    RTA_PAYLOAD(rta));
            route->have.oif = 1;
            break;

        case RTA_GATEWAY:
            memcpy(&route->gwy, RTA_DATA(rta), 
                    RTA_PAYLOAD(rta));
            route->have.gwy = 1;
            break;

        case RTA_PREFSRC:
            memcpy(&route->src, RTA_DATA(rta),
                    RTA_PAYLOAD(rta));
            route->have.src = 1;
            break;

        case RTA_DST:
            memcpy(&route->dst, RTA_DATA(rta),
                    RTA_PAYLOAD(rta));
            route->have.dst = 1;
            break;
        }
    }

    /* Success */
    retval = 0;

done:

    return retval;
}


/*!
 * @brief Initialize the route module
 */
int sstp_route_init(sstp_route_ctx_st **ctx)
{
    sstp_route_ctx_st *r = calloc(1, sizeof(sstp_route_ctx_st));
    if (!r)
    {
        goto done;
    }

    r->sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if (r->sock < 0)
    {
        goto done;
    }

    *ctx = r;
    return 0;

done:
    
    if (r)
    {
        if (r->sock)
            close(r->sock);
        free(r);
    }

    return -1;
}


/*!
 * @brief Cleanup the route context
 */
void sstp_route_done(sstp_route_ctx_st *ctx)
{
    if (!ctx)
    {
        return;
    }
    
    if (ctx->sock > 0)
    {
        close(ctx->sock);
        ctx->sock = 0;
    }

    free(ctx);
}


#else   /* #ifdef HAVE_NETLINK */


int sstp_route_replace(sstp_route_ctx_st *ctx, sstp_route_st *route)
{
    char cmd[255];
    FILE *proc = NULL;

    snprintf(cmd, sizeof(cmd), "ip route replace %s", route->ipcmd);
    proc = popen(cmd, "r");
    if (!proc) 
    {
        return -1;
    }

    pclose(proc);
}


int sstp_route_delete(sstp_route_ctx_st *ctx, sstp_route_st *route)
{
    char cmd[255];
    FILE *proc = NULL;

    snprintf(cmd, sizeof(cmd), "ip route delete %s", route->ipcmd);
    proc = popen(cmd, "r");
    if (!proc) 
    {
        return -1;
    }

    pclose(proc);
}


int sstp_route_get(sstp_route_ctx_st *ctx, struct sockaddr *dst,
        sstp_route_st *route)
{
    char cmd[255];
    char ip[INET6_ADDRSTRLEN];
    FILE *proc = NULL;
    char *ptr  = NULL;
    
    if (!sstp_ipaddr(dst, ip, sizeof(ip)))
    {
        return -1;
    }
    
    snprintf(cmd, sizeof(cmd), "ip route get %s", ip);

    proc = popen(cmd, "r");
    if (!proc) 
    {
        return -1;
    }

    ptr = fgets(route->ipcmd, sizeof(route->ipcmd), proc);
    if (!ptr) 
    {
        pclose(proc);
        return -1;
    }

    pclose(proc);
    return 0;
}


int sstp_route_init(sstp_route_ctx_st **ctx)
{
    /* No private context here */
    return 0;
}


void sstp_route_done(sstp_route_ctx_st *ctx)
{
    /* No private context here */
}

#endif  /* #ifdef HAVE_NETLINK */

#ifdef __SSTP_UNIT_TEST_ROUTE

const char *sstp_ipaddr(struct sockaddr *addr, char *buf, int len)
{
    const char *retval = NULL;

    switch (addr->sa_family)
    {
    case AF_INET:
    {
        struct sockaddr_in *in = (struct sockaddr_in*) addr;
        if (inet_ntop(AF_INET, &in->sin_addr, buf, len))
        {
            retval = buf;
        }
        break;
    }
    case AF_INET6:
    {
        struct sockaddr_in6 *in = (struct sockaddr_in6*) addr;
        if (inet_ntop(AF_INET6, &in->sin6_addr, buf, len))
        {
            retval = buf;
        }
        break;
    }
    default:
        break;
    }

    return retval;
}


int main(int argc, char *argv[])
{
    sstp_route_ctx_st *ctx = NULL;
    sstp_route_st route;
    struct sockaddr_in dst;
    int retval = EXIT_FAILURE;

#ifdef HAVE_NETLINK
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    char gw_ip [INET_ADDRSTRLEN];
#endif

    inet_pton(AF_INET, "4.4.2.2", &dst.sin_addr);
    dst.sin_family = AF_INET;

    if (sstp_route_init(&ctx))
    {
        printf("Could not initialize route object\n");
        goto done;
    }

    if (sstp_route_get(ctx, (struct sockaddr*) &dst, &route))
    {
        printf("Could not get route\n");
        goto done;
    }

#ifdef HAVE_NETLINK
    inet_ntop(AF_INET, &route.gwy, gw_ip, sizeof(gw_ip));
    inet_ntop(AF_INET, &route.src, src_ip, sizeof(src_ip));
    inet_ntop(AF_INET, &route.dst, dst_ip, sizeof(dst_ip));

    printf("Got route to %s from %s via %s dev %s\n",
        dst_ip, src_ip, gw_ip, route.ifname);
#endif

    /* Only if we run as root, test the add/del of the route */
    if (getuid() == 0)
    {
        if (sstp_route_replace(ctx, &route))
        {
            printf("Could not add route\n");
            goto done;
        }

#ifdef HAVE_NETLINK
        printf("Added route to %s via %s\n", dst_ip, 
                route.ifname);
#endif
        if (sstp_route_delete(ctx, &route))
        {
            printf("Could not del route\n");
            goto done;
        }

#ifdef HAVE_NETLINK
        printf("Deleted route to %s via %s\n", dst_ip, 
                route.ifname);
#endif
}

    retval = EXIT_SUCCESS;

done:

    sstp_route_done(ctx);

    return retval;
}

#endif /* #ifdef __SSTP_UNIT_TEST_ROUTE */

