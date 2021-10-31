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
    static constexpr const size_t INACTIVE_IDX_MASK = 0x80000000;

    void add_proc(std::unique_ptr< flow_processor > proc) {
        // Trivial for now

        size_t old_size = procs.size();

        procs.push_back(std::move(proc));

        proc_order.push_back(old_size);
    }

    uint16_t process(mbuf_vec_base& mbuf_vec) {
        for ( size_t idx : proc_order ) {
            if ( !mbuf_vec.size() ) {
                break;
            }

            if ( idx & INACTIVE_IDX_MASK )
                continue;

            uint16_t ret = procs[idx]->process(mbuf_vec);

            if ( ret < mbuf_vec.size() ) {
                mbuf_vec.free_back(mbuf_vec.size() - ret);
            }
        }

        return mbuf_vec.size();
    }

    void disable_stage(size_t idx) {
        proc_order[idx] |= INACTIVE_IDX_MASK;
    }

    void enable_stage(size_t idx) {
        proc_order[idx] &= ~INACTIVE_IDX_MASK;
    }

    std::vector< std::string > get_chain_names() const {
        std::vector< std::string > names;

        names.reserve(proc_order.size());

        for ( size_t idx : proc_order ) {
            names.push_back(procs[idx]->get_name());
        }

        return names;
    }

private:
    std::vector< std::unique_ptr< flow_processor > > procs;

    std::vector< size_t > proc_order;
};

class flow_distributor
{
public:
    flow_distributor(size_t max_ports, size_t num_queues, uint32_t ring_size);

    ~flow_distributor();

    void push_packets(uint16_t src_port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec);

    uint16_t pull_packets(uint16_t port_id, uint16_t queue_id, mbuf_vec_base& mbuf_vec);


private:
    size_t   max_ports;
    size_t   num_queues;
    uint32_t ring_size;

    std::vector< mbuf_ring > rings;
};


class flow_manager : noncopyable
{
public:
    static constexpr const size_t MAX_NUM_FLOWS = 8;

    flow_manager();

    ~flow_manager();


    void start();

    void stop();

private:
    void endpoint_work_callback(const std::vector< size_t >& endpoint_ids);

    void distributor_work_callback(const std::vector< size_t >& distributor_ids);

    struct private_data;

    std::unique_ptr< private_data > pdata;
};