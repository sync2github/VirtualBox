/* $Id: lwipopts.h 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * DHCP server - lwIP configuration options.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Dhcpd_lwipopts_h
#define VBOX_INCLUDED_SRC_Dhcpd_lwipopts_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>     /* For VBOX_STRICT. */
#include <iprt/mem.h>
#include <iprt/alloca.h>    /* This may include malloc.h (msc), which is something that has
                             * to be done before redefining any of the functions therein. */
#include <iprt/rand.h> /* see LWIP_RAND() definition */

/** Make lwIP use the libc malloc, or more precisely (see below) the IPRT
 * memory allocation functions. */
#define MEM_LIBC_MALLOC 1

/** Set proper memory alignment. */
#if HC_ARCH_BITS == 64
# define MEM_ALIGNMENT 8
#else
#define MEM_ALIGNMENT 4
#endif

/* Padding before Ethernet header to make IP header aligned */
#define ETH_PAD_SIZE 2

/* IP */
#define IP_REASSEMBLY 1
#define IP_REASS_MAX_PBUFS      128



/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high.

   NB: This is for PBUF_ROM and PBUF_REF pbufs only!

   Number of PBUF_POOL pbufs is controlled by PBUF_POOL_SIZE that,
   somewhat confusingly, breaks MEMP_NUM_* pattern.

   PBUF_RAM pbufs are allocated with mem_malloc (with MEM_LIBC_MALLOC
   set to 1 this is just system malloc), not memp_malloc.  */
#define MEMP_NUM_PBUF (1024 * 4)


/* MEMP_NUM_MLD6_GROUP: Maximum number of IPv6 multicast groups that
   can be joined.

   We need to be able to join solicited node multicast for each
   address (potentially different) and two groups for DHCP6.  All
   routers multicast is hardcoded in ip6.c and does not require
   explicit joining.  Provide also for a few extra groups just in
   case.  */
#define MEMP_NUM_MLD6_GROUP     (LWIP_IPV6_NUM_ADDRESSES + /* dhcp6 */ 2 + /* extra */ 8)


/* MEMP_NUM_TCPIP_MSG_*: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#define MEMP_NUM_TCPIP_MSG_API   128
#define MEMP_NUM_TCPIP_MSG_INPKT 1024

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        32

/* Pbuf options */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool.
   This is only for PBUF_POOL pbufs, primarily used by netif drivers.

   This should have been named with the MEMP_NUM_ prefix (cf.
   MEMP_NUM_PBUF for PBUF_ROM and PBUF_REF) as it controls the size of
   yet another memp_malloc() pool.  */
#define PBUF_POOL_SIZE          (1024 * 4)

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool.
   Use default that is based on TCP_MSS and PBUF_LINK_HLEN.  */
#undef PBUF_POOL_BUFSIZE

/** Turn on support for lightweight critical region protection. Leaving this
 * off uses synchronization code in pbuf.c which is totally polluted with
 * races. All the other lwip source files would fall back to semaphore-based
 * synchronization, but pbuf.c is just broken, leading to incorrect allocation
 * and as a result to assertions due to buffers being double freed. */
#define SYS_LIGHTWEIGHT_PROT 1

/** Attempt to get rid of htons etc. macro issues. */
#undef LWIP_PREFIX_BYTEORDER_FUNCS

#define LWIP_TCPIP_CORE_LOCKING_INPUT 0
#define LWIP_TCPIP_CORE_LOCKING 0

#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_COMPAT_MUTEX 1

#define LWIP_TCP 0
#define LWI_UDP 1
#define LWIP_ARP 1
#define ARP_PROXY 0
#define LWIP_ETHERNET 1

/* accept any->broadcast */
#define LWIP_IP_ACCEPT_UDP_PORT(port) ((port) == PP_NTOHS(/*DHCP_SERVER_PORT*/ 67))

#define LWIP_IPV6 0
#define LWIP_IPV6_FORWARD 0
#define LWIP_ND6_PROXY 0

#define LWIP_ND6_ALLOW_RA_UPDATES       (!LWIP_IPV6_FORWARD)
#define LWIP_IPV6_SEND_ROUTER_SOLICIT   (!LWIP_IPV6_FORWARD)
/* IPv6 autoconfig we don't need in proxy, but it required for very seldom cases
 * iSCSI over intnet with IPv6
 */
#define LWIP_IPV6_AUTOCONFIG            1
#if LWIP_IPV6_FORWARD /* otherwise use the default from lwip/opt.h */
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS   0
#endif

#define LWIP_IPV6_FRAG                  1

/**
 * aka Slirp mode.
 */
#define LWIP_CONNECTION_PROXY 0
#define IP_FORWARD            0

/* MEMP_NUM_SYS_TIMEOUT: the number of simultaneously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    16


/* this is required for IPv6 and IGMP needs */
#define LWIP_RAND() RTRandU32()

/* Debugging stuff. */
#ifdef DEBUG
# define LWIP_DEBUG
# include "lwip-log.h"

# define LWIP_PROXY_DEBUG LWIP_DBG_OFF
#endif /* DEBUG */

/* printf formatter definitions */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

/* Redirect libc memory alloc functions to IPRT. */
#define malloc(x) RTMemAlloc(x)
#define realloc(x,y) RTMemRealloc((x), (y))
#define free(x) RTMemFree(x)

/* Align VBOX_STRICT and LWIP_NOASSERT. */
#ifndef VBOX_STRICT
# define LWIP_NOASSERT 1
#endif

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_lwipopts_h */
