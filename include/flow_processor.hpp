/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common/common.hpp"
#include "common/lua_common.hpp"

#include "dpdk/dpdk_common.hpp"
#include "dpdk/dpdk_ethdev.hpp"

#include "flow_base.hpp"
#include "flow_builder_types.hpp"

class flow_proc_context
{
public:
    flow_proc_context(flow_dir direction, uint16_t endpoint_id) :
        direction(direction), related_endpoint_id(endpoint_id) {

    }

    __inline flow_dir get_direction() const noexcept {
        return direction;
    }

    __inline uint16_t get_related_endpoint_id() const noexcept {
        return related_endpoint_id;
    }

    __inline void set_related_endpoint_id(uint16_t endpoint_id) noexcept {
        related_endpoint_id = endpoint_id;
    }

private:
    flow_dir direction;

    uint16_t related_endpoint_id;
};

class flow_processor : public flow_node_base
{
public:
    flow_processor(std::string name, std::shared_ptr< dpdk_packet_mempool > mempool);

    ~flow_processor() override = default;

    virtual uint16_t process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) = 0;

    const std::vector<parameter_info>& get_exported_params() const noexcept {
        return exported_params;
    }

    virtual void init(const flow_proc_builder& builder) = 0;

protected:
    void export_param(std::string name, parameter_constraint_type constraint_type) {
        exported_params.push_back(parameter_info(std::move(name), constraint_type));
    }

private:
    std::vector<parameter_info> exported_params;
};


class ingress_packet_validator : public flow_processor
{
public:
    ingress_packet_validator(std::string name, std::shared_ptr< dpdk_packet_mempool > mempool, std::shared_ptr< flow_database > flow_database_ptr);

    ~ingress_packet_validator() override = default;

    uint16_t process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) override;

    void init(const flow_proc_builder& builder) override;

private:
    static bool handle_ipv4_packet(const uint8_t* ipv4_header_base, uint16_t l3_len, packet_private_info* packet_info);

};

class flow_classifier : public flow_processor
{
public:
    flow_classifier(std::string                      name,
                    std::shared_ptr< dpdk_packet_mempool >  mempool,
                    std::shared_ptr< flow_database > flow_database_ptr);

    ~flow_classifier() override = default;

    uint16_t process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) override;

    void init(const flow_proc_builder& builder) override;

private:
    std::shared_ptr< flow_database > flow_database_ptr;
};

class lua_packet_filter : public flow_processor
{
public:
    lua_packet_filter(std::string                      name,
                    std::shared_ptr< dpdk_packet_mempool >  mempool,
                    std::shared_ptr< flow_database > flow_database_ptr);

    ~lua_packet_filter() override = default;

    uint16_t process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) override;

    void init(const flow_proc_builder& builder) override;

private:
    std::shared_ptr< flow_database > flow_database_ptr;

    lua_engine lua;

    sol::function process_function;
};

std::unique_ptr< flow_processor > create_flow_processor(std::shared_ptr<flow_proc_builder> proc_builder,
                                                        const std::shared_ptr< dpdk_packet_mempool >& mempool,
                                                        const std::shared_ptr< flow_database >& flow_database);