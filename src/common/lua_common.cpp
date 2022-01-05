/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/lua_common.hpp>
#include "lua_utils.h"

lua_attachment_base::~lua_attachment_base() {
    if ( engine ) {
        engine->detach_all(*this);

        engine = nullptr;
    }
}


lua_engine::lua_engine() {

    state.set_exception_handler(lua_engine::_binding_exception_handler);

    state.set("DEBUG", (int) LOG_DEBUG);
    state.set("INFO", (int) LOG_INFO);
    state.set("WARN", (int) LOG_WARN);
    state.set("ERROR", (int) LOG_ERROR);

    state.set_function("log", [this](int level, std::string msg) { this->_binding_log(level, std::move(msg)); });

    state.script(std::string((const char*)___SRC_LUA_UTILS_LUA, ___SRC_LUA_UTILS_LUA_LEN), "internal_utils");
}

lua_engine::~lua_engine() {}

void lua_engine::load_stdlibs() {
    state.open_libraries(sol::lib::package, sol::lib::base, sol::lib::string, sol::lib::io, sol::lib::math, sol::lib::jit);
}

void lua_engine::execute(const std::string& script, const std::string& script_name) {
    std::lock_guard< std::mutex > guard(resource_lock);

    try {
        auto result = state.safe_script(script, script_name, sol::load_mode::text);

        if(!result.valid()) {
            sol::error err = result;

            log(LOG_ERROR, "Error while executing lua script {}", err.what());
        }
    } catch ( const std::exception& e ) {
        log(LOG_ERROR, "Error while executing lua script {}", e.what());
    }
}

void lua_engine::detach_all(lua_attachment_base& attachment) {
    std::lock_guard< std::mutex > guard(resource_lock);
}

void lua_engine::dump_state() {
    std::lock_guard< std::mutex > guard(resource_lock);

    for ( const auto& e : state ) {
        if ( e.first.valid() ) {
            std::string name = e.first.as< std::string >();

            log(LOG_INFO, "global element {}", name);
        }
    }
}


void lua_engine::_binding_log(int level, std::string msg) {
    log((log_level) level, "<lua> {}", msg);
}

int lua_engine::_binding_exception_handler(lua_State*                             L,
                                           sol::optional< const std::exception& > maybe_exception,
                                           sol::string_view                       description) {

    if ( maybe_exception ) {
        const std::exception& ex = *maybe_exception;

        log(LOG_ERROR, "lua exception: {}", ex.what());
    } else {
        log(LOG_ERROR, "lua msg: {}", std::string(description.data(), description.size()));
    }

    return sol::stack::push(L, description);
}
