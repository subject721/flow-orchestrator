/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once


#include "common/common.hpp"

#include "flow_base.hpp"
#include "flow_processor.hpp"

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
            if(!mbuf_vec.size()) {
                break;
            }

            if(idx & INACTIVE_IDX_MASK)
                continue;

            uint16_t ret = procs[idx]->process(mbuf_vec);

            if(ret < mbuf_vec.size()) {
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

    std::vector<std::string> get_chain_names() const {
        std::vector<std::string> names;

        names.reserve(proc_order.size());

        for(size_t idx : proc_order) {
            names.push_back(procs[idx]->get_name());
        }

        return names;
    }

private:
    std::vector< std::unique_ptr< flow_processor > > procs;

    std::vector< size_t >                            proc_order;
};


class flow_manager
{
public:


private:

};