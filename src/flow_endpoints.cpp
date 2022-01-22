/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_endpoints.hpp>


eth_dpdk_endpoint::eth_dpdk_endpoint(std::string                     name,
                                     std::shared_ptr< dpdk_packet_mempool > mempool,
                                     std::unique_ptr< dpdk_ethdev >  eth_dev) :
    flow_endpoint_base(std::move(name), (int)eth_dev->get_port_id(), std::move(mempool)), eth_dev(std::move(eth_dev))
#if TELEMETRY_ENABLED == 1
    ,metric_group(fmt::format("ep-{}", get_name()))
    ,tx_packets("tx_packets", metric_unit::PACKETS)
    ,rx_packets("rx_packets", metric_unit::PACKETS)
    ,tx_bytes("tx_bytes", metric_unit::BYTES)
    ,rx_bytes("rx_bytes", metric_unit::BYTES)
#endif
    {}

eth_dpdk_endpoint::~eth_dpdk_endpoint() {}

uint16_t eth_dpdk_endpoint::rx_burst(mbuf_vec_base& mbuf_vec) {
#ifdef DEBUG
    if(mbuf_vec.size()) {
        log(LOG_WARN, "eth_dpdk_endpoint::rx_burst mbuf_vec is not empty!");
    }
#endif

    uint16_t rx_count = get_ethdev()->rx_burst(0, mbuf_vec.end(), mbuf_vec.num_free_tail());

    mbuf_vec.grow_tail(rx_count);

#if TELEMETRY_ENABLED == 1
    rx_packets.add(rx_count);

    uint64_t num_bytes_local = 0;

    for(auto mbuf : mbuf_vec) {
        if(mbuf) {
            num_bytes_local += rte_pktmbuf_pkt_len(mbuf);
        }
    }

    rx_bytes.add(num_bytes_local);
#endif

    return rx_count;
}

uint16_t eth_dpdk_endpoint::tx_burst(mbuf_vec_base& mbuf_vec) {
    uint16_t tx_count = get_ethdev()->tx_burst(0, mbuf_vec.begin(), mbuf_vec.size());

#if TELEMETRY_ENABLED == 1
    tx_packets.add(tx_count);

    uint64_t num_bytes_local = 0;

    for(auto mbuf : mbuf_vec) {
        if(mbuf) {
            num_bytes_local += rte_pktmbuf_pkt_len(mbuf);
        }
    }

    tx_bytes.add(num_bytes_local);
#endif

    mbuf_vec.consume_front(tx_count);

    return tx_count;
}

void eth_dpdk_endpoint::start() {
    eth_dev->start();
}

void eth_dpdk_endpoint::stop() {
    eth_dev->stop();
}

std::unique_ptr< dpdk_ethdev > eth_dpdk_endpoint::detach_eth_dev() {
    return std::move(eth_dev);
}