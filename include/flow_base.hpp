/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include <rte_rcu_qsbr.h>
#include "common/common.hpp"
#include "common/network_utils.hpp"
#include "dpdk/dpdk_common.hpp"

#if TELEMETRY_ENABLED == 1
#include "flow_telemetry.hpp"
#endif

enum class flow_dir
{
    RX,
    TX
};

template < flow_dir DIR >
struct flow_dir_label;

template <>
struct flow_dir_label<flow_dir::RX>
{
    static const std::string name;
};

template <>
struct flow_dir_label<flow_dir::TX>
{
    static const std::string name;
};

enum class ExecutionPolicyType
{
    REDUCED_CORE_COUNT_POLICY
};

static __always_inline packet_private_info* get_private_packet_info(rte_mbuf* mbuf) {
    return reinterpret_cast< packet_private_info* >(rte_mbuf_to_priv(mbuf));
}


class flow_component : noncopyable
{
public:
    
    virtual ~flow_component() = default;

private:

};

class flow_node_base : public flow_component
{
public:
    flow_node_base(std::string name, std::shared_ptr< dpdk_packet_mempool > mempool);

    flow_node_base(flow_node_base&& other) noexcept;

    virtual ~flow_node_base() = default;

    const std::string& get_name() const noexcept;

    std::shared_ptr< dpdk_packet_mempool > get_mempool_shared() const {
        return mempool;
    }

protected:
    __inline dpdk_packet_mempool* get_mempool() {
        return mempool.get();
    }

private:
    std::string name;

    std::shared_ptr< dpdk_packet_mempool > mempool;
};

class flow_endpoint_base : public flow_node_base
{
public:
    flow_endpoint_base(std::string name, int port_num, std::shared_ptr< dpdk_packet_mempool > mempool);

    ~flow_endpoint_base() override = default;

    int get_port_num() const noexcept {
        return port_num;
    }

    virtual void start() = 0;

    virtual void stop() = 0;

    // Yes I'm aware that virtual functions have a performance impact
    // But for now I don't care
    virtual uint16_t rx_burst(mbuf_vec_base& mbuf_vec) = 0;

    virtual uint16_t tx_burst(mbuf_vec_base& mbuf_vec) = 0;

#if TELEMETRY_ENABLED == 1
    virtual void init_telemetry(telemetry_distributor& telemetry) {}
#endif

private:
    int port_num;
};



class flow_database : public flow_component
{
public:
    flow_database(size_t max_entries, std::vector<lcore_info> write_allowed_lcores);

    ~flow_database();

    bool lookup(flow_hash hash);

    flow_info_ipv4* get_or_create(flow_hash fhash, bool& created);

    void flow_purge_checkpoint(unsigned int lcore_id);

    void set_lcore_active(unsigned int lcore_id);

    void set_lcore_inactive(unsigned int lcore_id);

    size_t get_num_flows();

private:
    using lcore_table_state_t = std::array<uint32_t, RTE_MAX_LCORE>;

    size_t max_entries;

    std::atomic_size_t current_num_entries;

    std::vector<lcore_info> write_allowed_lcores;

    lcore_table_state_t lcore_state;

    std::unique_ptr<rte_mempool, mempool_deleter> mempool;

    std::unique_ptr<rte_rcu_qsbr, dpdk_malloc_deleter> rcu_state;

    size_t flow_table_memsize;

    std::unique_ptr<const rte_memzone, dpdk_memzone_deleter> table_memory;

};

template < class TFlowManager >
class flow_executor_base : noncopyable
{
public:
    using flow_manager_type = TFlowManager;

    using worker_callback_type = void (flow_manager_type::*)(const size_t*, size_t, std::atomic_bool&);

    flow_executor_base(flow_manager_type& flow_manager) : flow_manager(flow_manager) {}

    virtual ~flow_executor_base() = default;

    virtual void setup(const std::vector< int >& endpoint_sockets,
                       size_t                    num_distributors,
                       std::vector< lcore_info > p_available_lcores) = 0;

    virtual void start(worker_callback_type endpoint_callback, worker_callback_type distributor_callback) = 0;

    virtual void stop() = 0;

protected:
    flow_manager_type& get_flow_manager() noexcept {
        return flow_manager;
    }

private:
    flow_manager_type& flow_manager;
};


std::string get_flow_dir_name(flow_dir dir);
