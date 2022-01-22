/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_base.hpp>
#include <rte_compat.h>
#include <rte_malloc.h>

const std::string flow_dir_label< flow_dir::RX >::name = "rx";

const std::string flow_dir_label< flow_dir::TX >::name = "tx";

flow_node_base::flow_node_base(std::string name, std::shared_ptr< dpdk_packet_mempool > mempool) :
    name(std::move(name)), mempool(std::move(mempool)) {}

flow_node_base::flow_node_base(flow_node_base&& other) noexcept :
    name(std::move(other.name)), mempool(std::move(other.mempool)) {}

const std::string& flow_node_base::get_name() const noexcept {
    return name;
}


flow_endpoint_base::flow_endpoint_base(std::string name, int port_num, std::shared_ptr< dpdk_packet_mempool > mempool) :
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

static constexpr uint16_t FLOW_TABLE_KEYING_FACTOR = 8;

struct alignas(RTE_CACHE_LINE_SIZE) flow_table_entry_state
{

    flow_hash hash[FLOW_TABLE_KEYING_FACTOR];

    flow_info_ipv4* flow_info[FLOW_TABLE_KEYING_FACTOR];

    uint16_t lru_head;
};

flow_database::flow_database(size_t max_entries, std::vector< lcore_info > write_allowed_lcores) :
    max_entries(max_entries), write_allowed_lcores(write_allowed_lcores) {

    size_t element_size = sizeof(flow_info_ipv4);
    size_t cache_size   = 0;

    mempool = std::unique_ptr< rte_mempool, mempool_deleter >(rte_mempool_create("flowdatabase_pool",
                                                                                 max_entries,
                                                                                 element_size,
                                                                                 cache_size,
                                                                                 0,
                                                                                 nullptr,
                                                                                 nullptr,
                                                                                 nullptr,
                                                                                 nullptr,
                                                                                 SOCKET_ID_ANY,
                                                                                 MEMPOOL_F_NO_IOVA_CONTIG));

    std::memset(lcore_state.data(), 0, lcore_state.size() * sizeof(lcore_table_state_t::value_type));

    // We need to find the maximum lcore id for the rcu qsbr stuff
    auto lcore_max =
        std::reduce(write_allowed_lcores.begin(),
                    write_allowed_lcores.end(),
                    write_allowed_lcores.front(),
                    [](const auto& a, const auto& b) { return (a.get_lcore_id() > b.get_lcore_id()) ? a : b; });

    size_t rcu_state_size = rte_rcu_qsbr_get_memsize(lcore_max.get_lcore_id() + 1);

    rcu_state = std::unique_ptr< rte_rcu_qsbr, dpdk_malloc_deleter >(
        (rte_rcu_qsbr*) rte_zmalloc(nullptr, rcu_state_size, RTE_CACHE_LINE_SIZE));


    if ( rte_rcu_qsbr_init(rcu_state.get(), lcore_max.get_lcore_id() + 1) ) {
        throw std::runtime_error("could not init rcu state");
    }

    flow_table_memsize = sizeof(flow_table_entry_state) * max_entries;

    table_memory = std::unique_ptr< const rte_memzone, dpdk_memzone_deleter >(rte_memzone_reserve(
        "flow_table_zone", flow_table_memsize, SOCKET_ID_ANY, RTE_MEMZONE_2MB | RTE_MEMZONE_SIZE_HINT_ONLY));

    if ( !table_memory ) {
        throw std::runtime_error("could not allocate flow table memory zone");
    }

    std::memset(table_memory->addr, 0, flow_table_memsize);
}

flow_database::~flow_database() {}

bool flow_database::lookup(flow_hash hash) {
    return false;
}

flow_info_ipv4* flow_database::get_or_create(flow_hash fhash, bool& created) {

    unsigned int lcore_id = rte_lcore_id();

    rte_rcu_qsbr* rcu = rcu_state.get();

    flow_table_entry_state* flow_table_data = (flow_table_entry_state*) table_memory.get()->addr;

    flow_info_ipv4* flow_entry = nullptr;

    // Dummy key reduction
    uint32_t reduced_key = (fhash % max_entries);

    {
        uint16_t target_index;

        flow_table_entry_state* dst_state = flow_table_data + reduced_key;

        rte_rcu_qsbr_lock(rcu, lcore_id);

        uint16_t index = dst_state->lru_head;

        do {
            if ( dst_state->hash[index] == fhash ) {
                flow_entry   = dst_state->flow_info[index];
                target_index = index;
                break;
            }

            ++index;

            if ( index >= FLOW_TABLE_KEYING_FACTOR ) {
                index -= FLOW_TABLE_KEYING_FACTOR;
            }
        } while ( index != dst_state->lru_head );


        rte_rcu_qsbr_unlock(rcu, lcore_id);

        if ( !flow_entry ) {
            struct flow_info_ipv4* oldest_entry = nullptr;

            auto qs_token = rte_rcu_qsbr_start(rcu);

            rte_rcu_qsbr_quiescent(rcu, lcore_id);

            rte_mempool_get(mempool.get(), (void**) &flow_entry);

            if ( likely(flow_entry != nullptr) ) {

                uint16_t old_lru_head = dst_state->lru_head;

                uint16_t new_lru_head;

                if ( old_lru_head > 0 ) {
                    new_lru_head = old_lru_head - 1;
                } else {
                    new_lru_head = FLOW_TABLE_KEYING_FACTOR - 1;
                }

                oldest_entry                  = dst_state->flow_info[new_lru_head];
                dst_state->hash[new_lru_head] = fhash;
                rte_compiler_barrier();
                dst_state->flow_info[new_lru_head] = flow_entry;

                rte_rcu_qsbr_check(rcu, qs_token, true);

                rte_mempool_put(mempool.get(), (void*) oldest_entry);

                dst_state->lru_head = new_lru_head;

                created = true;
            }
        }
    }

    if ( flow_entry ) {
        flow_entry->last_used = rte_get_tsc_cycles();
    }

    return flow_entry;
}

void flow_database::flow_purge_checkpoint(unsigned int lcore_id) {
    rte_rcu_qsbr_quiescent(rcu_state.get(), lcore_id);
}

void flow_database::set_lcore_active(unsigned int lcore_id) {
    rte_rcu_qsbr_thread_register(rcu_state.get(), lcore_id);

    lcore_state[lcore_id] = 1;

    rte_rcu_qsbr_thread_online(rcu_state.get(), lcore_id);
}

void flow_database::set_lcore_inactive(unsigned int lcore_id) {
    rte_rcu_qsbr_thread_offline(rcu_state.get(), lcore_id);

    lcore_state[lcore_id] = 0;

    rte_rcu_qsbr_thread_unregister(rcu_state.get(), lcore_id);
}