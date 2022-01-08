/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_config.hpp>
#include <common/file_utils.hpp>


const std::string flow_dir_label< flow_dir::RX >::name = "rx";

const std::string flow_dir_label< flow_dir::TX >::name = "tx";


flow_init_proc& flow_init_proc::param(const std::string& key, const std::string& value) {

    params[key] = value;

    return *this;
}

std::shared_ptr< flow_init_proc > flow_init_proc::next(std::shared_ptr< flow_init_proc > p) {
    next_proc = std::move(p);

    return next_proc;
}


std::shared_ptr< flow_init_proc > flow_init_endpoint::add_rx_proc(std::shared_ptr< flow_init_proc > p) {
    if ( !first_rx_proc ) {
        first_rx_proc = std::move(p);

        return first_rx_proc;
    } else {
        std::shared_ptr< flow_init_proc > current = first_rx_proc;

        while ( current->get_next_proc() ) {
            current = current->get_next_proc();
        }

        return current->next(std::move(p));
    }
}

std::shared_ptr< flow_init_proc > flow_init_endpoint::add_tx_proc(std::shared_ptr< flow_init_proc > p) {
    if ( !first_tx_proc ) {
        first_tx_proc = std::move(p);

        return first_tx_proc;
    } else {
        std::shared_ptr< flow_init_proc > current = first_tx_proc;

        while ( current->get_next_proc() ) {
            current = current->get_next_proc();
        }

        return current->next(std::move(p));
    }
}

class flow_creation_lua_extension : public lua_engine_extension< flow_creation_lua_extension >
{
public:
    flow_creation_lua_extension(init_script_handler& init_handler) :
        lua_engine_extension("flow"), init_handler(init_handler) {}

    ~flow_creation_lua_extension() override {
        ut_endpoint.unregister();
        ut_proc.unregister();
    }

private:
    void init() override {

        ut_endpoint = new_usertype< flow_init_endpoint >(
            "endpoint", sol::no_constructor, sol::base_classes, sol::bases< flow_init_node >());

        ut_endpoint["name"]                        = &flow_init_node::name;
        ut_endpoint[sol::meta_function::to_string] = &flow_init_node::name;
        ut_endpoint["add_rx_proc"]                 = &flow_init_endpoint::add_rx_proc;
        ut_endpoint["add_tx_proc"]                 = &flow_init_endpoint::add_tx_proc;
        ut_endpoint["port_num"]                    = &flow_init_endpoint::get_port_num;

        ut_proc = new_usertype< flow_init_proc >(
            "processor", sol::no_constructor, sol::base_classes, sol::bases< flow_init_node >());

        ut_proc["name"]                        = &flow_init_node::name;
        ut_proc[sol::meta_function::to_string] = &flow_init_node::name;
        ut_proc["next"]                        = &flow_init_proc::next;

        set_function< std::shared_ptr< flow_init_proc > >(
            "proc", std::function< std::shared_ptr< flow_init_proc >(const std::string&) >([](const std::string& name) {
                return std::make_shared< flow_init_proc >(name);
            }));
    }

    init_script_handler& init_handler;

    sol::usertype< flow_init_endpoint > ut_endpoint;
    sol::usertype< flow_init_proc >     ut_proc;
};

init_script_handler::init_script_handler() {
    lua.load_stdlibs();

    lua.set_function(
        "set_option",
        std::function< void(std::string, std::string) >(
            [this](const std::string& name, const std::string& value) { cb_set_config_var(name, value); }));

    lua.set_function("get_option", std::function< std::string(std::string) >([this](const std::string& name) {
                         return cb_get_config_var(name);
                     }));
}

void init_script_handler::load_init_script(const std::string& filename) {
    std::string script_content = load_file_as_string(filename);

    lua.get().collect_garbage();

    lua.execute(script_content, filename);

    program_name = lua.get< std::string >("program_name").value_or(filename);
}

flow_program init_script_handler::build_program(
    std::vector< std::unique_ptr< flow_endpoint_base > > available_endpoints) {

    try {
        flow_creation_lua_extension flow_ext(*this);

        lua.load_extension(flow_ext);

        std::vector< std::shared_ptr< flow_init_endpoint > > endpoint_list;

        std::transform(
            available_endpoints.begin(), available_endpoints.end(), std::back_inserter(endpoint_list), [](auto& ep) {
                return std::make_shared< flow_init_endpoint >(ep->get_name(), ep->get_port_num());
            });

        lua.call< void >("init", endpoint_list);

        flow_program prog(program_name);

        for ( size_t endpoint_idx = 0; endpoint_idx < available_endpoints.size(); ++endpoint_idx ) {
            const auto& ep = endpoint_list[endpoint_idx];

            std::shared_ptr< dpdk_mempool > current_mempool = available_endpoints[endpoint_idx]->get_mempool_shared();

            std::shared_ptr< flow_init_proc > current = ep->get_first_rx_proc();

            auto& flow = prog.add_flow(fmt::format("flow-{}", ep->get_port_num()));

            if(current) {

                std::string s;

                handle_flow(flow,
                            *available_endpoints[endpoint_idx],
                            prog.get_flow_database(),
                            ep->get_first_rx_proc(),
                            flow_dir::RX);

                handle_flow(flow,
                            *available_endpoints[endpoint_idx],
                            prog.get_flow_database(),
                            ep->get_first_tx_proc(),
                            flow_dir::TX);
            } else {
                log(LOG_INFO, "Flow for endpoint {} is empty", ep->name());
            }

            flow.set_endpoint(std::move(available_endpoints[endpoint_idx]));
        }

        return prog;

    } catch ( const std::exception& e ) {

        throw std::runtime_error(fmt::format("Could not call init function of init script : {}", e.what()));
    }
}

void init_script_handler::handle_flow(flow_config&                      flow,
                                      flow_endpoint_base&               endpoint,
                                      std::shared_ptr< flow_database >  flow_database,
                                      std::shared_ptr< flow_init_proc > proc_info,
                                      flow_dir                          dir) {
    std::shared_ptr< flow_init_proc > current = std::move(proc_info);

    std::shared_ptr< dpdk_mempool > current_mempool = endpoint.get_mempool_shared();

    std::string s;

    while ( current ) {
        if ( s.empty() ) {
            s = fmt::format("{} -> {}", endpoint.get_name(), current->name());
        } else {
            s = fmt::format("{} -> {}", s, current->name());
        }


        flow.add_proc(create_flow_processor(current->name(),
                                            fmt::format("{}-{}", endpoint.get_name(), current->name()),
                                            current_mempool,
                                            flow_database),
                      dir);

        current = current->get_next_proc();
    }

    log(LOG_INFO, "{} chain for endpoint {}: {}", get_flow_dir_name(dir), endpoint.get_name(), s);
}

void init_script_handler::cb_set_config_var(const std::string& name, const std::string& value) {}

std::string init_script_handler::cb_get_config_var(const std::string& name) const {
    return {};
}

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