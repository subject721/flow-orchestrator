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
        // TODO: Enable passing of socket id
        rings.emplace_back(fmt::format("fd-ring-{}", index), 0, ring_size);
    }
}

flow_distributor::~flow_distributor() {}

void flow_distributor::push_packets(uint16_t src_port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec) {

    for ( uint16_t packet_index = 0; packet_index < mbuf_vec.size(); ++packet_index ) {

        rte_mbuf* current_mbuf = mbuf_vec.begin()[packet_index];

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
    explicit private_data(uint16_t num_queues) : active(false), distributor(MAX_NUM_FLOWS, num_queues, 128) {}

    std::atomic_bool active;

    std::shared_ptr<flow_database> flow_database;

    flow_distributor distributor;

    size_t num_endpoints;

    std::array< std::optional< packet_proc_flow >, MAX_NUM_FLOWS >     rx_proc_flows;
    std::array< std::unique_ptr< flow_endpoint_base >, MAX_NUM_FLOWS > proc_endpoints;

    std::unique_ptr<flow_executor_base<flow_manager>> executor;
};

flow_manager::flow_manager() {

}

flow_manager::~flow_manager() {
    if(pdata) {
        stop();

        pdata.reset();
    }
}

void flow_manager::load(flow_program prog) {
    if(pdata && pdata->active.load()) {
        throw std::runtime_error("cannot replace an active flow program");
    }

    pdata = std::make_unique<private_data>(1);

    pdata->flow_database = prog.get_flow_database();

    size_t index = 0;

    // Sick iteration! Whoop
    for(auto& flow : prog) {

        pdata->proc_endpoints[index] = flow.detach_endpoint();

        pdata->rx_proc_flows[index].emplace();

        for(auto& proc : flow_proc_iterator<flow_dir::RX>(flow)) {
            pdata->rx_proc_flows[index]->add_proc(std::move(proc));
        }

        for(auto& proc : flow_proc_iterator<flow_dir::TX>(flow)) {

        }

        auto chain_names = pdata->rx_proc_flows[index]->get_chain_names();

        log(LOG_INFO, "Loaded processing chain for endpoint {}: ", pdata->proc_endpoints[index]->get_name());
        for(const auto& name : chain_names) {
            log(LOG_INFO, "proc : {}", name);
        }

        ++index;

    }

    pdata->num_endpoints = index;
}

void flow_manager::start() {
    if(!pdata) {
        throw std::runtime_error("no program loaded");
    }

    if(pdata->active.load()) {
        throw std::runtime_error("flow program already active");
    }

    pdata->executor = create_executor(*this, ExecutionPolicyType::REDUCED_CORE_COUNT_POLICY);

    std::vector<int> endpoint_numa_ids;

    for(auto& ep : pdata->proc_endpoints) {
        if(ep) {
            endpoint_numa_ids.push_back(rte_eth_dev_socket_id(ep->get_port_num()));

            ep->start();
        }
    }

    pdata->executor->setup(endpoint_numa_ids, 1, lcore_info::get_available_worker_lcores());

    pdata->active.store(true);

    pdata->executor->start(&flow_manager::endpoint_work_callback, &flow_manager::distributor_work_callback);
}

void flow_manager::stop() {
    if(!pdata) {
        throw std::runtime_error("no program loaded");
    }

    if(!pdata->active.load()) {
        return;
    }

    pdata->executor->stop();

    for(auto& ep : pdata->proc_endpoints) {
        if(ep) {
            ep->stop();
        }
    }

    pdata->active.store(false);
}

void flow_manager::endpoint_work_callback(const std::vector< size_t >& endpoint_ids) {
    private_data* p = pdata.get();

    static_mbuf_vec<BURST_SIZE> mbuf_vec;

    auto lcore_id = rte_lcore_id();

    for(size_t ep_id : endpoint_ids) {
        //log(LOG_INFO, "lcore{} : endpoint callback - handling endpoint {}", lcore_id, ep_id);

        auto& ep = p->proc_endpoints[ep_id];

        ep->rx_burst(mbuf_vec);

        //log(LOG_INFO, "lcore{} : pulled {} packets from endpoint {}", lcore_id, num_bufs, ep_id);

        p->rx_proc_flows[ep_id]->process(mbuf_vec);

        p->distributor.push_packets(ep_id, 0, mbuf_vec);
    }

    if(mbuf_vec.size()) {
        log(LOG_WARN, "mbuf_vec has size != 0 after endpoint handling!");
    }

    //rte_delay_ms(100);
}

void flow_manager::distributor_work_callback(const std::vector< size_t >& distributor_ids) {
    private_data* p = pdata.get();

    static_mbuf_vec<BURST_SIZE> mbuf_vec;

    auto lcore_id = rte_lcore_id();

    //log(LOG_INFO, "lcore{} : distributor callback - collecting packets", lcore_id);

    for(size_t index = 0; index < p->num_endpoints; ++index) {
        uint16_t num_pulled_bufs = p->distributor.pull_packets(index, 0, mbuf_vec);

        //log(LOG_INFO, "lcore{} : transmitting {} packets on endpoint {}", lcore_id, num_pulled_bufs, index);

        p->proc_endpoints[index]->tx_burst(mbuf_vec);

        // This will free all remaining mbufs if there are any
        mbuf_vec.free();

//        if(num_tx_bufs < num_pulled_bufs) {
//            mbuf_vec.free_back(num_pulled_bufs - num_tx_bufs);
//        }
    }

    //rte_delay_ms(100);
}
