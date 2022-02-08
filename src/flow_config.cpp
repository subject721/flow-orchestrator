/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_config.hpp>
#include <common/file_utils.hpp>


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

        ut_endpoint = new_usertype< flow_endpoint_builder >(
            "endpoint", sol::no_constructor, sol::base_classes, sol::bases< flow_builder_node >());

        ut_endpoint["get_instance_name"]           = &flow_builder_node::get_instance_name;
        ut_endpoint[sol::meta_function::to_string] = &flow_builder_node::get_instance_name;
        ut_endpoint["add_rx_proc"]                 = &flow_endpoint_builder::add_rx_proc;
        ut_endpoint["add_tx_proc"]                 = &flow_endpoint_builder::add_tx_proc;
        ut_endpoint["port_num"]                    = &flow_endpoint_builder::get_port_num;

        ut_proc = new_usertype< flow_proc_builder >(
            "processor", sol::no_constructor, sol::base_classes, sol::bases< flow_builder_node >());

        ut_proc["get_instance_name"]           = &flow_builder_node::get_instance_name;
        ut_proc[sol::meta_function::to_string] = &flow_builder_node::get_instance_name;
        ut_proc["next"]                        = &flow_proc_builder::next;
        ut_proc["set_param"]                   = &flow_proc_builder::set_param;
        ut_proc["get_param"]                   = &flow_proc_builder::get_param;

        set_function< std::shared_ptr< flow_proc_builder > >(
            "proc",
            std::function< std::shared_ptr< flow_proc_builder >(std::string, std::optional< std::string >) >(
                [](std::string class_name, std::optional< std::string > instance_name_opt) {
                    std::string instance_name = instance_name_opt.value_or(class_name);

                    return std::make_shared< flow_proc_builder >(instance_name, class_name);
                }));
    }

    init_script_handler& init_handler;

    sol::usertype< flow_endpoint_builder > ut_endpoint;
    sol::usertype< flow_proc_builder >     ut_proc;
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
    std::vector< std::unique_ptr< flow_endpoint_base > > available_endpoints,
    const std::shared_ptr< flow_database >&              flow_database) {

    std::string current_context_name;

    try {
        flow_creation_lua_extension flow_ext(*this);

        current_context_name = "loading extension";

        lua.load_extension(flow_ext);

        std::vector< std::shared_ptr< flow_endpoint_builder > > endpoint_list;

        std::transform(
            available_endpoints.begin(), available_endpoints.end(), std::back_inserter(endpoint_list), [](auto& ep) {
                return std::make_shared< flow_endpoint_builder >(ep->get_name(), ep->get_port_num());
            });

        current_context_name = "calling init()";

        lua.call< void >("init", endpoint_list);

        flow_program prog(program_name, flow_database);

        for ( size_t endpoint_idx = 0; endpoint_idx < available_endpoints.size(); ++endpoint_idx ) {
            const auto& ep = endpoint_list[endpoint_idx];

            std::shared_ptr< dpdk_packet_mempool > current_mempool =
                available_endpoints[endpoint_idx]->get_mempool_shared();

            std::shared_ptr< flow_proc_builder > current = ep->get_first_rx_proc();

            auto& flow = prog.add_flow(fmt::format("flow-{}", ep->get_port_num()));

            if ( current ) {

                std::string s;

                current_context_name = "loading rx flow";

                handle_flow(
                    flow, *available_endpoints[endpoint_idx], flow_database, ep->get_first_rx_proc(), flow_dir::RX);

                current_context_name = "loading tx flow";

                handle_flow(
                    flow, *available_endpoints[endpoint_idx], flow_database, ep->get_first_tx_proc(), flow_dir::TX);
            } else {
                log(LOG_INFO, "Flow for endpoint {} is empty", ep->get_instance_name());
            }

            flow.set_endpoint(std::move(available_endpoints[endpoint_idx]));
        }

        return prog;

    } catch ( const std::exception& e ) {

        throw std::runtime_error(fmt::format("Error while executing init script in context [{}] : {}", current_context_name, e.what()));
    }
}

void init_script_handler::handle_flow(flow_config&                         flow,
                                      flow_endpoint_base&                  endpoint,
                                      std::shared_ptr< flow_database >     flow_database,
                                      std::shared_ptr< flow_proc_builder > proc_info,
                                      flow_dir                             flow_direction) {
    std::shared_ptr< flow_proc_builder > current = std::move(proc_info);

    std::shared_ptr< dpdk_packet_mempool > current_mempool = endpoint.get_mempool_shared();

    std::string s;

    while ( current ) {
        if ( s.empty() ) {
            s = fmt::format("{} -> {}", endpoint.get_name(), current->get_instance_name());
        } else {
            s = fmt::format("{} -> {}", s, current->get_instance_name());
        }

        flow.add_proc(create_flow_processor(current, current_mempool, flow_database), flow_direction);

        current = current->get_next_proc();
    }

    log(LOG_INFO, "{} chain for endpoint {}: {}", get_flow_dir_name(flow_direction), endpoint.get_name(), s);
}

void init_script_handler::cb_set_config_var(const std::string& name, const std::string& value) {}

std::string init_script_handler::cb_get_config_var(const std::string& name) const {
    return {};
}
