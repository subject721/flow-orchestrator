#pragma once

#include "common.hpp"

#include <rte_ether.h>
#include <rte_ip.h>

using flow_hash = uint64_t;

struct flow_info_ipv4
{
    uint32_t src_addr;
    uint32_t dst_addr;
    
    uint16_t src_port;
    uint16_t dst_port;

    uint8_t ipv4_proto;

    uint16_t mark;
};


// NOTE: It is blatantly assumed the host endianness is always little-endian.
//
template < uint16_t ETH_TYPE_HOST >
struct ether_type_info
{
    static constexpr const uint16_t   ether_type_host = ETH_TYPE_HOST;

    static constexpr const rte_be16_t ether_type_be   = (ether_type_host >> 8) | ((ether_type_host & 0x00ffU) << 8);
};



