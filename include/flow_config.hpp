/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once


#include "flow_base.hpp"
#include "flow_endpoints.hpp"
#include "flow_processor.hpp"
#include "flow_builder_types.hpp"

#include "common/lua_common.hpp"

#include <list>
#include <map>


struct dev_info
{
    std::optional< std::string > dev_type_str;
    std::optional< std::string > dev_id_str;
    std::optional< std::string > dev_options_str;
};

template < class T >
struct is_endpoint : std::bool_constant< std::is_base_of_v< flow_endpoint_base, T > >
{
};

template < class T >
struct is_flow_proc : std::bool_constant< std::is_base_of_v< flow_processor, T > >
{
};


class flow_config
{
public:
    explicit flow_config(std::string flow_name) : flow_name(std::move(flow_name)) {}

    const std::string& get_name() const noexcept {
        return flow_name;
    }

    template < class T >
    flow_config& set_endpoint(std::unique_ptr< T > n_endpoint) {
        static_assert(is_endpoint< T >::value, "Type must be derived from flow_endpoint_base");
        this->endpoint = std::move(n_endpoint);

        return *this;
    }

    template < class T >
    flow_config& add_proc(std::unique_ptr< T > proc, flow_dir fdir) {
        static_assert(is_flow_proc< T >::value, "Type must be derived from flow_processor");

        if ( fdir == flow_dir::RX ) {
            rx_procs.push_back(std::move(proc));
        } else {
            tx_procs.push_back(std::move(proc));
        }

        return *this;
    }

    std::unique_ptr< flow_endpoint_base > detach_endpoint() {
        return std::move(endpoint);
    }

    std::list< std::unique_ptr< flow_processor > >::iterator rx_proc_begin() {
        return rx_procs.begin();
    }

    std::list< std::unique_ptr< flow_processor > >::iterator rx_proc_end() {
        return rx_procs.end();
    }

    std::list< std::unique_ptr< flow_processor > >::iterator tx_proc_begin() {
        return tx_procs.begin();
    }

    std::list< std::unique_ptr< flow_processor > >::iterator tx_proc_end() {
        return tx_procs.end();
    }

private:
    std::string flow_name;

    std::unique_ptr< flow_endpoint_base > endpoint;

    std::list< std::unique_ptr< flow_processor > > rx_procs;
    std::list< std::unique_ptr< flow_processor > > tx_procs;
};

template < flow_dir DIR >
struct flow_proc_iterator;

template <>
struct flow_proc_iterator< flow_dir::RX >
{
    flow_proc_iterator(flow_config& cfg) : cfg(cfg) {}

    decltype(auto) begin() {
        return cfg.rx_proc_begin();
    }

    decltype(auto) end() {
        return cfg.rx_proc_end();
    }

    flow_config& cfg;
};

template <>
struct flow_proc_iterator< flow_dir::TX >
{
    flow_proc_iterator(flow_config& cfg) : cfg(cfg) {}

    decltype(auto) begin() {
        return cfg.tx_proc_begin();
    }

    decltype(auto) end() {
        return cfg.tx_proc_end();
    }

    flow_config& cfg;
};

class flow_program
{
public:
    using flow_cfg_iterator       = std::list< flow_config >::iterator;
    using flow_cfg_const_iterator = std::list< flow_config >::const_iterator;

    explicit flow_program(std::string program_name, std::shared_ptr< flow_database > flow_database_ptr) :
        program_name(std::move(program_name)), flow_database_ptr(std::move(flow_database_ptr)) {}

    const std::string& get_name() const noexcept {
        return program_name;
    }

    flow_config& add_flow(std::string flow_name) {
        auto it = flow_configs.insert(flow_configs.end(), flow_config {std::move(flow_name)});

        return *it;
    }

    flow_cfg_iterator begin() {
        return flow_configs.begin();
    }

    flow_cfg_iterator end() {
        return flow_configs.end();
    }

    size_t get_num_flow_configs() const noexcept {
        return flow_configs.size();
    }

    std::shared_ptr< flow_database > get_flow_database() const {
        return flow_database_ptr;
    }

private:
    std::string program_name;

    std::list< flow_config > flow_configs;

    std::shared_ptr< flow_database > flow_database_ptr;
};

class init_script_handler : noncopyable
{
public:
    init_script_handler();

    void load_init_script(const std::string& filename);

    flow_program build_program(std::vector< std::unique_ptr< flow_endpoint_base > > available_endpoints,
                               const std::shared_ptr< flow_database >&              flow_database);

private:
    void handle_flow(flow_config&                         flow,
                     flow_endpoint_base&                  endpoint,
                     std::shared_ptr< flow_database >     flow_database,
                     std::shared_ptr< flow_proc_builder > proc_info,
                     flow_dir                             flow_direction);

    void cb_set_config_var(const std::string& name, const std::string& value);

    std::string cb_get_config_var(const std::string& name) const;

    std::string program_name;

    lua_engine lua;
};
