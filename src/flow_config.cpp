/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_config.hpp>
#include <common/file_utils.hpp>

init_script_handler::init_script_handler() {
    lua.load_stdlibs();
}

void init_script_handler::load_init_script(const std::string& filename) {
    std::string script_content = load_file_as_string(filename);

    lua.execute(script_content);

    lua.set_function("set_option", std::function<void(std::string, std::string)>([this](const std::string& name, const std::string& value){
        cb_set_config_var(name, value);
    }));

    lua.set_function("get_option", std::function<std::string(std::string)>([this](const std::string& name){
        return cb_get_config_var(name);
    }));

    try {
        lua.call<int>("init");
    } catch(const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

flow_program init_script_handler::build_program(const std::vector< std::shared_ptr< flow_endpoint_base > >& available_endpoints) {
    return flow_program("");
}

void init_script_handler::cb_set_config_var(const std::string& name, const std::string& value) {

}

std::string init_script_handler::cb_get_config_var(const std::string& name) const {
    return {};
}