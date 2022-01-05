/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_endpoints.hpp>


eth_dpdk_endpoint::eth_dpdk_endpoint(std::string                     name,
                                     std::shared_ptr< dpdk_mempool > mempool,
                                     std::unique_ptr< dpdk_ethdev >  eth_dev) :
    flow_endpoint_base(std::move(name), (int)eth_dev->get_port_id(), std::move(mempool)), eth_dev(std::move(eth_dev)) {}

eth_dpdk_endpoint::~eth_dpdk_endpoint() {}

uint16_t eth_dpdk_endpoint::rx_burst(mbuf_vec_base& mbuf_vec) {
    uint16_t rx_count = get_ethdev()->rx_burst(0, mbuf_vec.data(), mbuf_vec.num_free_tail());

    mbuf_vec.set_size(rx_count);

    return rx_count;
}

uint16_t eth_dpdk_endpoint::tx_burst(mbuf_vec_base& mbuf_vec) {
    uint16_t tx_count = get_ethdev()->tx_burst(0, mbuf_vec.data(), mbuf_vec.size());

    mbuf_vec.consume_front(tx_count);

    return tx_count;
}

std::unique_ptr< dpdk_ethdev > eth_dpdk_endpoint::detach_eth_dev() {
    return std::move(eth_dev);
}