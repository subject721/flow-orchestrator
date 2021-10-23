#pragma once


#include "flow_base.hpp"
#include "flow_endpoints.hpp"
#include "flow_processor.hpp"

#include <list>


enum class flow_dir
{
    RX,
    TX
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

private:
    std::string flow_name;

    std::unique_ptr< flow_endpoint_base > endpoint;

    std::list< std::unique_ptr< flow_processor > > rx_procs;
    std::list< std::unique_ptr< flow_processor > > tx_procs;
};

class flow_program
{
public:
    using flow_cfg_iterator = std::list<flow_config>::iterator;
    using flow_cfg_const_iterator = std::list<flow_config>::const_iterator;

    explicit flow_program(std::string program_name) : program_name(std::move(program_name)) {

    }

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

private:
    std::string program_name;

    std::list< flow_config > flow_configs;
};

