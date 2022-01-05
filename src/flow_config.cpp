/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_config.hpp>
#include <common/file_utils.hpp>

class flow_init_node
{
public:
    flow_init_node() {}

    flow_init_node(const std::string& id) : id(id) {}

    const std::string& name() const noexcept {
        return id;
    }

private:
    std::string id;
};

class flow_init_proc : public flow_init_node
{
public:
    flow_init_proc() {}

    flow_init_proc(const std::string& name) : flow_init_node(name) {}

    flow_init_proc& param(const std::string& key, const std::string& value) {

        params[key] = value;

        return *this;
    }

    std::shared_ptr< flow_init_proc > next(std::shared_ptr< flow_init_proc > p) {
        next_proc = std::move(p);

        return next_proc;
    }

    std::shared_ptr< flow_init_proc > get_next_proc() const {
        return next_proc;
    }

private:
    std::map< std::string, std::string > params;

    std::shared_ptr< flow_init_proc > next_proc;
};

class flow_init_endpoint : public flow_init_node
{
public:
    flow_init_endpoint(const std::string& id, int port_num) : flow_init_node(id), port_num(port_num) {}

    std::shared_ptr< flow_init_proc > next(std::shared_ptr< flow_init_proc > p) {
        next_proc = std::move(p);

        return next_proc;
    }

    std::shared_ptr< flow_init_proc > first_proc() const {
        return next_proc;
    }

    int get_port_num() const noexcept {
        return port_num;
    }

private:
    std::shared_ptr< flow_init_proc > next_proc;

    int port_num;
};

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
        ut_endpoint["next"]                        = &flow_init_endpoint::next;
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

    program_name = lua.get<std::string>("program_name").value_or(filename);
}

flow_program init_script_handler::build_program(
    const std::vector< std::shared_ptr< flow_endpoint_base > >& available_endpoints) {

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

        for ( auto& ep : endpoint_list ) {
            log(LOG_INFO, "Chain for endpoint {}", ep->name());

            std::shared_ptr< flow_init_proc > current = ep->first_proc();

            std::string s;

            auto& flow = prog.add_flow(fmt::format("flow-{}", ep->get_port_num()));



            while ( current ) {
                if ( s.empty() ) {
                    s = fmt::format("{} -> {}", ep->name(), current->name());
                } else {
                    s = fmt::format("{} -> {}", s, current->name());
                }

                current = current->get_next_proc();
            }

            log(LOG_INFO, "{}", s);
        }

        return prog;

    } catch ( const std::exception& e ) {

        throw std::runtime_error(fmt::format("Could not call init function of init script : {}", e.what()));
    }
}

void init_script_handler::cb_set_config_var(const std::string& name, const std::string& value) {}

std::string init_script_handler::cb_get_config_var(const std::string& name) const {
    return {};
}