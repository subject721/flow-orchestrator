/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common/common.hpp"
#include <common/network_utils.hpp>
#include "dpdk/dpdk_common.hpp"



class flow_node_base : noncopyable
{
public:
    flow_node_base(std::string name, std::shared_ptr<dpdk_mempool> mempool);

    virtual ~flow_node_base() = default;

    const std::string& get_name() const noexcept;

protected:

    __inline dpdk_mempool* get_mempool() {
        return mempool.get();
    }

private:
    std::string name;

    std::shared_ptr<dpdk_mempool> mempool;
};

class flow_endpoint_base : public flow_node_base
{
public:
    flow_endpoint_base(std::string name, std::shared_ptr<dpdk_mempool> mempool);

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