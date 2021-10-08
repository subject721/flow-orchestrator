/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common.hpp"

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>

enum ip_next_proto : uint8_t
{
    IP_PROTO_ICMP  = 0x01U,
    IP_PROTO_IGMP  = 0x02U,
    IP_PROTO_IPIIP = 0x04U,
    IP_PROTO_TCP   = 0x06U,
    IP_PROTO_UDP   = 0x11U,
    IP_PROTO_GRE   = 0x2fU,
    IP_PROTO_ESP   = 0x32U,
    IP_PROTO_AH    = 0x33U,
};

constexpr const uint16_t PORT_ID_BROADCAST = 0xffff;

using flow_hash = uint64_t;

struct flow_info_ipv4
{
    uint32_t       src_addr;
    uint32_t       dst_addr;

    uint16_t       src_port;
    uint16_t       dst_port;

    rte_ether_addr ether_src;
    rte_ether_addr ether_dst;

    uint8_t        ipv4_proto;


    uint64_t       mark;

    __inline bool get_mark_bit(uint8_t idx) const noexcept {
        return (mark & (1 << idx));
    }

    __inline void set_mark_bit(uint8_t idx) noexcept {
        mark |= (1 << idx);
    }
};

struct packet_private_info
{
    // non-null if packet belongs to known flow
    flow_info_ipv4* flow_info;


    uint16_t        src_endpoint_id;
    uint16_t        dst_endpoint_id;

    uint16_t        l3_offset;

    uint16_t        l4_offset;

    uint16_t        ether_type;

    uint16_t        vlan;

    uint8_t         ipv4_type;

    uint16_t        ipv4_len;

    bool            is_fragment;
};

// NOTE: It is blatantly assumed the host endianness is always little-endian.
//
template < uint16_t ETH_TYPE_HOST >
struct ether_type_info
{
    static constexpr const uint16_t   ether_type_host = ETH_TYPE_HOST;

    static constexpr const rte_be16_t ether_type_be   = (ether_type_host >> 8) | ((ether_type_host & 0x00ffU) << 8);
};


static __inline void get_ether_header_info(rte_ether_hdr* ether_header, uint16_t* len, uint16_t* tci, uint16_t* type) {
    *len = sizeof(rte_ether_hdr);

    if(ether_header->ether_type == ether_type_info<RTE_ETHER_TYPE_VLAN>::ether_type_be) {
        *len += 4;

        rte_vlan_hdr* vlan_header = reinterpret_cast<rte_vlan_hdr*>(ether_header + 1);

        *tci = vlan_header->vlan_tci;
        *type = vlan_header->eth_proto;
    } else {
        *tci = 0;
        *type = ether_header->ether_type;
    }
}

static __inline void init_flow_info_ipv4(flow_info_ipv4*      flow_info,
                                         const rte_ether_hdr* ether_hdr,
                                         const rte_ipv4_hdr*  ipv4_hdr) {
    rte_ether_addr_copy(&ether_hdr->s_addr, &flow_info->ether_src);
    rte_ether_addr_copy(&ether_hdr->d_addr, &flow_info->ether_dst);

    flow_info->src_addr   = ipv4_hdr->src_addr;
    flow_info->dst_addr   = ipv4_hdr->dst_addr;

    flow_info->ipv4_proto = ipv4_hdr->next_proto_id;
}


bool calc_flow_hash(rte_mbuf* mbuf, flow_hash* flow_hash);
