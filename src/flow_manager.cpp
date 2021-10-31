/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_manager.hpp>

#include <flow_executor.hpp>

#include <optional>


flow_distributor::flow_distributor(size_t max_ports, size_t num_queues, uint32_t ring_size) :
    max_ports(max_ports), num_queues(num_queues), ring_size(ring_size) {

    size_t total_num_rings = max_ports * num_queues;

    rings.reserve(total_num_rings);

    for ( auto index : seq(0, total_num_rings) ) {
        rings.emplace_back(fmt::format("fd-ring-{}", index), 0, ring_size);
    }
}

flow_distributor::~flow_distributor() {}

void flow_distributor::push_packets(uint16_t src_port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec) {

    for ( uint16_t packet_index = 0; packet_index < mbuf_vec.size(); ++packet_index ) {

        rte_mbuf* current_mbuf = mbuf_vec.data()[packet_index];

        auto* packet_info = reinterpret_cast< packet_private_info* >(rte_mbuf_to_priv(current_mbuf));

        if ( unlikely(packet_info->dst_endpoint_id == PORT_ID_BROADCAST) ) {
            for ( size_t port_id = queue_id; port_id < rings.size(); port_id += num_queues ) {
                rte_mbuf_refcnt_update(current_mbuf, 1);

                if ( !rings[port_id].enqueue_single(current_mbuf) ) {
                    rte_pktmbuf_free(current_mbuf);
                }
            }
        } else {

            size_t ridx = (packet_info->dst_endpoint_id * num_queues) + queue_id;

            if ( !rings[ridx].enqueue_single(current_mbuf) ) {
                rte_pktmbuf_free(current_mbuf);
            }
        }
    }

    mbuf_vec.consume();
}

uint16_t flow_distributor::pull_packets(uint16_t port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec) {
    size_t ridx = (port_id * num_queues) + queue_id;

    return rings[ridx].dequeue(mbuf_vec);
}


struct flow_manager::private_data
{
    explicit private_data(uint16_t num_queues) : distributor(MAX_NUM_FLOWS, num_queues, 128) {}


    flow_distributor distributor;

    std::array< std::optional< packet_proc_flow >, MAX_NUM_FLOWS >  proc_flows;
    std::array< std::optional< eth_dpdk_endpoint >, MAX_NUM_FLOWS > proc_endpoints;
};

flow_manager::flow_manager() {}

flow_manager::~flow_manager() {}

void flow_manager::start() {}

void flow_manager::stop() {}

void flow_manager::endpoint_work_callback(const std::vector< size_t >& endpoint_ids) {}

void flow_manager::distributor_work_callback(const std::vector< size_t >& distributor_ids) {}
