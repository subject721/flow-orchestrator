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
#include <list>
#include <map>

class lua_engine;

class lua_attachment_base : noncopyable
{
public:
    virtual ~lua_attachment_base();

private:
    lua_engine* engine;

    friend class lua_engine;
};

template < class T >
class lua_engine_extension
{
public:
    using extension_upper_type = T;

    lua_engine_extension(const std::string& extension_name) : engine(nullptr), extension_name(extension_name) {

    }

    virtual ~lua_engine_extension();

protected:

    template <class... TArgs>
    void set(TArgs&&... args);

    template < class TRet, class... TArgs >
    void set_function(const std::string& name, std::function< TRet(TArgs...) > func);

    virtual void init() = 0;

    const sol::table& get_extension_data() const noexcept {
        return extension_data;
    }

    sol::table& get_extension_data() noexcept {
        return extension_data;
    }

    template < class TUserType, class... TArgs >
    sol::usertype<TUserType> new_usertype(const std::string& name, TArgs&&... args);

private:

    void make_active(lua_engine* new_engine);


    void clear() {

        extension_data.clear();
    }

    lua_engine* engine;

    std::string extension_name;

    sol::table extension_data;

    // For auto removal
    std::map<std::string, sol::metatable> usertype_metatables;

    friend class lua_engine;
};


class lua_engine : noncopyable
{
public:
    lua_engine();

    ~lua_engine();

    void load_stdlibs();

    void execute(const std::string& script, const std::string& script_name = std::string());

    template <class T>
    void load_extension(lua_engine_extension<T>& extension) {
        extension.make_active(this);
    }

    template < class TRet, class... TArgs >
    auto call(const char* func_name, TArgs&&... args) -> std::conditional_t<std::is_same_v<TRet, void>, void, std::optional<TRet>> {
        sol::protected_function f = state[func_name];

        if ( f.valid() ) {
            sol::protected_function_result ret = f.call(std::forward< TArgs >(args)...);


            if ( ret.valid() ) {
                if constexpr (!std::is_same_v<TRet, void>) {
                    return ret.get< TRet >();
                }
            } else {
                sol::error err = ret;

                throw std::runtime_error(fmt::format("error: {}", err.what()));
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
        state.set(name, std::forward< T >(v));
    }

    template < class TRet, class... TArgs >
    void set_function(const std::string& name, std::function< TRet(TArgs...) > func) {
        state.set_function(name, func);
    }

    void detach_all(lua_attachment_base& attachment);

    void dump_state();

    const sol::state& get() const noexcept {
        return state;
    }

    sol::state& get() noexcept {
        return state;
    }

private:
    void _binding_log(int level, std::string msg);

    static int _binding_exception_handler(lua_State*                             L,
                                          sol::optional< const std::exception& > maybe_exception,
                                          sol::string_view                       description);

    sol::state state;

    std::mutex resource_lock;

    std::set< lua_attachment_base* > attachments;
};


template < class T >
lua_engine_extension<T>::~lua_engine_extension() {
    clear();
}

template < class T >
template < class... TArgs >
void lua_engine_extension<T>::set(TArgs&&... args)  {
    extension_data.set(std::forward<TArgs>(args)...);
}

template < class T >
template < class TRet, class... TArgs >
void lua_engine_extension<T>::set_function(const std::string& name, std::function< TRet(TArgs...) > func) {
    extension_data.set_function(name, func);
}

template < class T >
template < class TUserType, class... TArgs >
sol::usertype<TUserType> lua_engine_extension<T>::new_usertype(const std::string& name, TArgs&&... args) {
    sol::usertype<TUserType> ut = extension_data.new_usertype<TUserType>(name, std::forward<TArgs>(args)...);

    //usertype_metatables.insert(std::make_pair(name, ut));


    return ut;
}

template < class T >
void lua_engine_extension<T>::make_active(lua_engine* new_engine) {
    if(engine) {
        throw std::runtime_error("extension instance already active");
    }

    engine = new_engine;

    extension_data = engine->get().create_table();

    init();

    engine->get().set(extension_name, extension_data);
}


