/*!
 * @brief Definitions for PPP packet dump
 *
 * @file sstp-ppp.h
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

#ifndef __SSTP_PPP_H__
#define __SSTP_PPP_H__

/* 
 * PPP Codes
 */
#define FSM_CONFREQ         1
#define FSM_CONFACK         2
#define FSM_CONFNAK         3
#define FSM_CONFREJ         4
#define FSM_TERMREQ         5
#define FSM_TERMACK         6
#define FSM_CODEREJ         7
#define FSM_PROTOREJ        8
#define FSM_ECHOREQ         9
#define FSM_ECHOREP         10
#define FSM_DISCARDREQ      11
#define FSM_RESETREQ        14
#define FSM_RESETACK        15
#define FSM_FIRST           FSM_CONFREQ
#define FSM_LAST            FSM_RESETACK

/* 
 * LCP Options 
 */
#define CI_VENDOR           0       /* Vendor Specific */
#define CI_MRU              1       /* Maximum Receive Unit */
#define CI_ASYNCMAP         2       /* Async Control Character Map */
#define CI_AUTH             3       /* Authentication Type */
#define CI_QUALITY          4       /* Quality Protocol */
#define CI_MAGIC            5       /* Magic Number */
#define CI_PCOMP            7       /* Protocol Field Compression */
#define CI_ACCOMP           8       /* Address/Control Field Compression */
#define CI_FCSALTERN        9       /* FCS-Alternatives */
#define CI_SDP              10      /* Self-Describing-Pad */
#define CI_NUMBERED         11      /* Numbered-Mode */
#define CI_CALLBACK         13      /* callback */
#define CI_MRRU             17      /* max reconstructed receive unit; multilink */
#define CI_SSNHF            18      /* short sequence numbers for multilink */
#define CI_EPDISC           19      /* endpoint discriminator */
#define CI_MPPLUS           22      /* Multi-Link-Plus-Procedure */
#define CI_LDISC            23      /* Link-Discriminator */
#define CI_LCPAUTH          24      /* LCP Authentication */
#define CI_COBS             25      /* Consistent Overhead Byte Stuffing */
#define CI_PREFELIS         26      /* Prefix Elision */
#define CI_MPHDRFMT         27      /* MP Header Format */
#define CI_I18N             28      /* Internationalization */
#define CI_SDL              29      /* Simple Data Link */

/*
 * IPCP Options
 */
#define CI_ADDRS            1
#define CI_COMPRESSTYPE     2
#define CI_ADDR             3
#define CI_MS_DNS1          129
#define CI_MS_WINS1         130
#define CI_MS_DNS2          131
#define CI_MS_WINS2         132

#define IPCP_VJ_COMP        0x002d
#define IPCP_VJ_COMP_OLD    0x0037

/*
 * @brief CCP options 4-15 is unassigned, 255 is reserved.
 */
#define CI_PREDICT1         1
#define CI_PREDICT2         2
#define CI_PUDDLE           3
#define CI_HPPPC            16
#define CI_SELZS            17
#define CI_MPPC             18
#define CI_GFZA             19
#define CI_V42C             20
#define CI_BSD              21

/*
 * CHAP messages
 */
#define CHAP_CHALLENGE  1
#define CHAP_RESPONSE   2
#define CHAP_SUCCESS    3
#define CHAP_FAILURE    4


/*
 * MPPE bits
 */
#define MPPE_C_BIT              0x01    /* MPPC */
#define MPPE_D_BIT              0x10    /* Obsolete, usage unknown */
#define MPPE_L_BIT              0x20    /* 40-bit */
#define MPPE_S_BIT              0x40    /* 128-bit */
#define MPPE_M_BIT              0x80    /* 56-bit, not supported */

#define MPPE_H_BIT              0x01    /* Stateless (in a different byte) */

typedef struct 
{
    uint8_t code;
    uint8_t id;
    uint16_t len;

} ppp_hdr_st;

typedef struct 
{
    uint8_t type;
    uint8_t len;

} ppp_opt_st;

#endif /* #ifdef __SSTP_PPP_H__ */
