/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_base.hpp>


const std::string flow_dir_label< flow_dir::RX >::name = "rx";

const std::string flow_dir_label< flow_dir::TX >::name = "tx";

flow_node_base::flow_node_base(std::string name, std::shared_ptr< dpdk_mempool > mempool) :
    name(std::move(name)), mempool(std::move(mempool)) {}

flow_node_base::flow_node_base(flow_node_base&& other) noexcept :
    name(std::move(other.name)), mempool(std::move(other.mempool)) {}

const std::string& flow_node_base::get_name() const noexcept {
    return name;
}


flow_endpoint_base::flow_endpoint_base(std::string name, int port_num, std::shared_ptr< dpdk_mempool > mempool) :
    flow_node_base(std::move(name), std::move(mempool)), port_num(port_num) {}


std::string get_flow_dir_name(flow_dir dir) {
    switch ( dir ) {
        case flow_dir::RX:
            return flow_dir_label< flow_dir::RX >::name;
        case flow_dir::TX:
            return flow_dir_label< flow_dir::TX >::name;
        default:
            throw std::invalid_argument("invalid direction");
    }
}