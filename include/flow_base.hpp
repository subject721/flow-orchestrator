/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common/common.hpp"
#include "common/network_utils.hpp"
#include "dpdk/dpdk_common.hpp"

enum class ExecutionPolicyType
{
    REDUCED_CORE_COUNT_POLICY
};

class flow_node_base : noncopyable
{
public:
    flow_node_base(std::string name, std::shared_ptr< dpdk_mempool > mempool);

    flow_node_base(flow_node_base&& other) noexcept;

    virtual ~flow_node_base() = default;

    const std::string& get_name() const noexcept;

protected:
    __inline dpdk_mempool* get_mempool() {
        return mempool.get();
    }

private:
    std::string name;

    std::shared_ptr< dpdk_mempool > mempool;
};

class flow_endpoint_base : public flow_node_base
{
public:
    flow_endpoint_base(std::string name, std::shared_ptr< dpdk_mempool > mempool);

    ~flow_endpoint_base() override = default;

    // Yes I'm aware that virtual functions have a performance impact
    // But for now I don't care
    virtual uint16_t rx_burst(mbuf_vec_base& mbuf_vec) = 0;

    virtual uint16_t tx_burst(mbuf_vec_base& mbuf_vec) = 0;

private:
};

class flow_database : noncopyable
{
public:
    flow_database(size_t max_entries);

    ~flow_database();

    bool lookup(flow_hash hash);


private:
};

template < class TFlowManager >
class flow_executor_base : noncopyable
{
public:
    using flow_manager_type = TFlowManager;

    using worker_callback_type = void (flow_manager_type::*)(const std::vector< size_t >&);

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