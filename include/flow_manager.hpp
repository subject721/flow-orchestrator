/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once


#include "common/common.hpp"

#include "flow_base.hpp"
#include "flow_processor.hpp"
#include "flow_endpoints.hpp"
#include "flow_config.hpp"

class packet_proc_flow : noncopyable
{
public:
    static constexpr uint32_t INACTIVE_IDX_MASK = 0x80000000U;
    static constexpr uint32_t FLOW_TERMINATOR = 0x0fffffffU;

    static constexpr uint32_t MAX_FLOW_LENGTH = 16;

    packet_proc_flow() : current_flow_length(0) {
        for(auto& e : proc_order) {
            e = FLOW_TERMINATOR;
        }
    }

    void add_proc(std::unique_ptr< flow_processor > proc) {
        // Trivial for now

        size_t old_size = procs.size();

        procs.push_back(std::move(proc));

        proc_order[current_flow_length] = old_size;

        ++current_flow_length;
    }

    __inline uint16_t process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) {
        for ( size_t idx = 0; idx < MAX_FLOW_LENGTH; ++idx ) {

            uint32_t proc_id = proc_order[idx];

            if(proc_id == FLOW_TERMINATOR) {
                break;
            }

            if ( !mbuf_vec.size() ) {
                break;
            }

            if ( unlikely( proc_id & INACTIVE_IDX_MASK ))
                continue;

            uint16_t ret = procs[proc_id]->process(mbuf_vec, ctx);

            if ( ret < mbuf_vec.size() ) {
                mbuf_vec.free_back(mbuf_vec.size() - ret);
            }
        }

        return mbuf_vec.size();
    }

    void disable_stage(size_t idx) {
        proc_order[idx] |= INACTIVE_IDX_MASK;

        rte_wmb();
    }

    void enable_stage(size_t idx) {
        proc_order[idx] &= ~INACTIVE_IDX_MASK;

        rte_wmb();
    }

    std::vector< std::string > get_chain_names() const {
        std::vector< std::string > names;

        names.reserve(current_flow_length);

        for ( size_t idx = 0; idx < current_flow_length; ++idx ) {
            uint32_t proc_id = proc_order[idx];

            if(proc_id == FLOW_TERMINATOR)
                break;

            if(proc_id & INACTIVE_IDX_MASK)
                continue;

            names.push_back(procs[proc_id]->get_name());
        }

        return names;
    }

private:
    std::vector< std::unique_ptr< flow_processor > > procs;

    std::array< uint32_t, MAX_FLOW_LENGTH > proc_order;

    size_t current_flow_length;
};

class flow_distributor
{
public:
    flow_distributor(size_t max_ports, size_t num_queues, uint32_t ring_size);

    ~flow_distributor();

    void push_packets(uint16_t src_port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec);

    uint16_t pull_packets(uint16_t port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec);

    void set_num_active_ports(size_t num_ports) {
        num_active_ports = num_ports;
    }

private:
    size_t   max_ports;
    size_t   num_queues;
    size_t   num_active_ports;
    uint32_t ring_size;


    std::vector< mbuf_ring > rings;
};


class flow_manager : noncopyable
{
public:
    static constexpr const size_t MAX_NUM_FLOWS = 8;

    static constexpr const size_t BURST_SIZE = 32;

    flow_manager();

    ~flow_manager();

    void load(flow_program prog);

    void start();

    void stop();

private:
    void endpoint_work_callback(const size_t* endpoint_ids, size_t num_endpoint_ids, std::atomic_bool& run_state);

    void distributor_work_callback(const size_t* distributor_ids, size_t num_distributor_ids, std::atomic_bool& run_state);

    struct private_data;

    std::unique_ptr< private_data > pdata;
};