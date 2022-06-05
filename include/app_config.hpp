/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common/common.hpp"

#include <filesystem>
#include <functional>
#include <limits>
#include <type_traits>
#include <sstream>
#include <vector>

struct no_limits
{
    template < class T >
    bool is_valid(const T& value) const noexcept {
        return true;
    }

    template < class T >
    void correct(T& attribute_unused value) const {}
};

template < class T >
struct min_max_limits
{
    using value_type = T;

    static_assert(std::is_arithmetic_v< value_type >, "T must be an arithmetic type.");

    value_type min_value;
    value_type max_value;

    constexpr min_max_limits(value_type min_value = std::numeric_limits< value_type >::min(),
                             value_type max_value = std::numeric_limits< value_type >::max()) noexcept :
        min_value(min_value), max_value(max_value) {}

    bool is_valid(const value_type& value) const noexcept {
        if ( value < min_value )
            return false;
        else if ( value > max_value )
            return false;
        return true;
    }

    void correct(value_type& value) const {
        value = std::max(std::min(max_value, value), min_value);
    }
};

template < class TLimits, class TValue >
struct is_valid_limits_type
{
    template < class TL, class TV >
    static auto check(const TL& l, TV& v)
        -> std::enable_if_t< std::is_same_v< decltype(l.is_valid((const TV&) v)), bool > &&
                                 std::is_same_v< decltype(l.correct(v)), void >,
                             std::true_type > {
        return {};
    }

    template < class TL, class TV >
    static std::false_type check(...) {
        static_assert(sizeof(TV));
        return {};
    }


    static constexpr const bool value =
        decltype(check< TLimits, TValue >(std::declval< TLimits >(), std::declval< TValue& >()))::value;
};

struct config_param_base
{
    config_param_base(std::string name) : name(std::move(name)) {}

    virtual ~config_param_base() = default;

    virtual void set_from_string(const std::string& str) = 0;

    std::string name;
};

template < class T, class TLimitTraits = no_limits >
struct config_param : public config_param_base
{
    using value_type = T;

    using limits_type = TLimitTraits;

    static_assert(is_valid_limits_type< limits_type, value_type >::value,
                  "Value validation traits type is not compatible with given value type.");

    template < class TInit >
    config_param(TInit&& init_val, std::string name, limits_type limits = {}) :
        config_param_base(std::move(name)), value(std::forward< TInit >(init_val)), limits(std::move(limits)) {}

    ~config_param() override = default;

    template < class TV >
    void set(TV&& new_value) {
        if ( !limits.is_valid(new_value) ) {
            value_type tmp(std::forward< TV >(new_value));

            limits.correct(tmp);

            value = std::move(tmp);
        } else {
            value = std::forward< TV >(new_value);
        }
    }

    void set_from_string(const std::string& str) override {
        std::istringstream isstr(str);

        value_type tmp;

        isstr >> tmp;

        set(std::move(tmp));
    }

    value_type  value;
    limits_type limits;
};


class app_config
{
public:
    app_config() noexcept;

    void load_from_toml(const std::filesystem::path& cfg_file_path);

    size_t get_primary_pkt_allocator_capacity() const noexcept {
        return primary_pkt_allocator_capacity.value;
    }

    size_t get_primary_pkt_allocator_cache_size() const noexcept {
        return primary_pkt_allocator_cache_size.value;
    }

    size_t get_flowtable_capacity() const noexcept {
        return flowtable_capacity.value;
    }


private:
    std::vector< std::reference_wrapper< config_param_base > > dataplane_config_params;

    config_param< size_t, min_max_limits< size_t > > primary_pkt_allocator_capacity;
    config_param< size_t, min_max_limits< size_t > > primary_pkt_allocator_cache_size;
    config_param< size_t, min_max_limits< size_t > > flowtable_capacity;
};
