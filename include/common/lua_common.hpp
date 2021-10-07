/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once


#include "common.hpp"

#include <sol/sol.hpp>

#include <optional>
#include <string_view>
#include <set>

class lua_engine;

class lua_attachment_base : noncopyable
{
public:
    virtual ~lua_attachment_base();

private:
    lua_engine* lua_engine;

    friend class lua_engine;
};


class lua_engine : noncopyable
{
public:
    lua_engine();

    ~lua_engine();

    void load_stdlibs();

    void execute(const std::string& str);

    template < class TRet, class... TArgs >
    std::optional< TRet > call(const char* func_name, TArgs&&... args) {
        sol::protected_function f = state[func_name];

        if ( f.valid() ) {
            sol::protected_function_result ret = f.call(std::forward< TArgs >(args)...);

            if ( ret.valid() ) {
                return ret.get< TRet >();
            } else {
                return {};
            }
        } else {
            throw std::runtime_error("function does not exist");
        }
    }

    template < class T >
    std::optional< T > get(const char* name) {
        auto v = state[name];

        return v.get_or(std::optional< T > {});
    }

    template < class T >
    void set(const char* name, T&& v) {
        static_assert(std::is_fundamental_v< T > || std::is_pod_v< T >, "T must be trivial data type");


        state.set(name, std::forward< T >(v));
    }

    template < class TRet, class... TArgs >
    void set_function(const std::string& name, std::function< TRet(TArgs...) > func) {
        state.set_function(name, func);
    }

    void detach_all(lua_attachment_base& attachment);

    void dump_state();

private:
    void                             _binding_log(int level, std::string msg);

    static int                       _binding_exception_handler(lua_State*                             L,
                                                                sol::optional< const std::exception& > maybe_exception,
                                                                sol::string_view                       description);

    sol::state                       state;

    std::mutex                       resource_lock;

    std::set< lua_attachment_base* > attachments;
};
