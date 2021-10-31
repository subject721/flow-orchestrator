/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common.hpp"


template < class T >
struct factory_element
{
    using type = T;

    explicit factory_element(const char* name) : name(name) {}

    template < class... TCtorArgs >
    static std::true_type ctor_trycall(decltype(T(std::declval< TCtorArgs >()...))*);

    template < class... TCtorArgs >
    static std::false_type ctor_trycall(...);

    template < class... TCtorArgs >
    static constexpr bool is_callable() {
        return decltype(ctor_trycall< TCtorArgs... >((T*) nullptr))::value;
    }

    template < class... TCtorArgs >
    T construct(TCtorArgs&&... ctor_args) {
        if constexpr ( is_callable< decltype(std::forward< TCtorArgs >(ctor_args))... >() ) {
            return {std::forward< TCtorArgs >(ctor_args)...};
        } else {
            throw std::runtime_error("invalid constructor call");
        }
    }

    template < class TTarget, class... TCtorArgs >
    void construct_and_assign(TTarget& target, TCtorArgs&&... ctor_args) {
        if constexpr ( std::is_assignable_v< TTarget, T > && (sizeof(T) == sizeof(TTarget)) &&
                       is_callable< decltype(std::forward< TCtorArgs >(ctor_args))... >() ) {
            target = T(std::forward< TCtorArgs >(ctor_args)...);
        } else {
            throw std::runtime_error("invalid constructor call");
        }
    }

    template < class TTarget, class... TCtorArgs >
    void construct_and_assign(std::unique_ptr< TTarget >& target, TCtorArgs&&... ctor_args) {
        if constexpr ( is_callable< decltype(std::forward< TCtorArgs >(ctor_args))... >() ) {
            target = std::make_unique< T >(std::forward< TCtorArgs >(ctor_args)...);
        } else {
            throw std::runtime_error("invalid constructor call");
        }
    }

    template < class TTarget, class... TCtorArgs >
    void construct_and_assign(std::shared_ptr< TTarget >& target, TCtorArgs&&... ctor_args) {
        if constexpr ( is_callable< decltype(std::forward< TCtorArgs >(ctor_args))... >() ) {
            target = std::make_shared< T >(std::forward< TCtorArgs >(ctor_args)...);
        } else {
            throw std::runtime_error("invalid constructor call");
        }
    }

    const char* name;
};

template < class TTuple, size_t I = 0, size_t N = std::tuple_size_v< TTuple > >
struct factory_tuple_iterator
{
    template < class TTarget, class... TArgs >
    static void check_and_create(TTuple& t, const std::string& target_name, TTarget& target, TArgs&&... args) {
        if ( target_name == std::get< I >(t).name ) {
            std::get< I >(t).construct_and_assign(target, std::forward< TArgs >(args)...);

            return;
        } else {
            factory_tuple_iterator< TTuple, I + 1, N >::check_and_create(
                t, target_name, target, std::forward< TArgs >(args)...);
        }
    }
};

template < class TTuple, size_t N >
struct factory_tuple_iterator< TTuple, N, N >
{
    template < class TTarget, class... TArgs >
    static void check_and_create(TTuple& t, const std::string& target_name, TTarget& target, TArgs&&... args) {
        throw std::runtime_error("invalid factory name");
    }
};

template < class TBaseClass, class... TCreationTypes >
struct factory_collection
{
    using base_class = TBaseClass;

    using factory_container_type = std::tuple< factory_element< TCreationTypes >... >;

    factory_container_type factories;

    explicit factory_collection(factory_container_type f) : factories(std::move(f)) {}

    template < class TNext >
    auto append(const char* name) {
        return factory_collection< TBaseClass, TCreationTypes..., TNext >(
            std::tuple_cat(factories, std::tuple< factory_element< TNext > >(name)));
    }

    template < class TTarget, class... TArgs >
    void construct_and_assign(TTarget& target, const std::string& target_name, TArgs&&... args) {
        factory_tuple_iterator< factory_container_type >::check_and_create(
            factories, target_name, target, std::forward< TArgs >(args)...);
    }
};

template < class TBaseClass >
auto create_factory() {
    return factory_collection< TBaseClass >({});
}
