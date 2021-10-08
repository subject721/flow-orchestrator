/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/network_utils.hpp>

#include <rte_jhash.h>


static constexpr uint32_t FLOW_HASH_SEED = 0x623fca21U;

bool calc_flow_hash(rte_mbuf* mbuf, flow_hash* flow_hash) {

    const rte_ether_hdr* ether_header = rte_pktmbuf_mtod_offset(mbuf, struct rte_ether_hdr*, 0);

    const packet_private_info* packet_info = reinterpret_cast< const packet_private_info* >(rte_mbuf_to_priv(mbuf));

    // Interpret the two mac addresses as three 32bit integers to make hashing more easy:
    const auto* mac_addr_data32b = (const uint32_t*) ether_header->d_addr.addr_bytes;

    uint32_t h = rte_jhash_3words(mac_addr_data32b[0], mac_addr_data32b[1], mac_addr_data32b[2], FLOW_HASH_SEED);

    if ( ether_header->ether_type == ether_type_info< RTE_ETHER_TYPE_IPV4 >::ether_type_be ) {
        const rte_ipv4_hdr* ipv4_header = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr*, packet_info->l3_offset);

        h ^= rte_jhash_2words(ipv4_header->dst_addr, ipv4_header->src_addr, FLOW_HASH_SEED);

    } else {
        return false;
    }

    if ( packet_info->ipv4_type == IP_PROTO_UDP || packet_info->ipv4_type == IP_PROTO_TCP ) {

        // Again for convenience we combine both ports into one 32bit value
        const uint32_t* port_info = rte_pktmbuf_mtod_offset(mbuf, const uint32_t*, packet_info->l4_offset);

        h ^= rte_jhash_1word(*port_info, FLOW_HASH_SEED);
    }

    *flow_hash = h;

    return true;
}