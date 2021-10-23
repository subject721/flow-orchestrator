/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cctype>

#include <mutex>
#include <memory>
#include <atomic>
#include <variant>
#include <filesystem>
#include <limits>
#include <algorithm>

#include <config.h>

#include <fmt/format.h>

class noncopyable
{
public:
    noncopyable()                   = default;

    noncopyable(const noncopyable&) = delete;

    void operator=(const noncopyable&) = delete;
};

template < class T >
struct is_clonable
{
    template < class U >
    using check_impl = std::is_same<decltype(std::declval<U>().clone()), U>;

    template < class U >
    static auto check(U&& u) -> std::enable_if_t<check_impl<std::decay_t<U>>::value, std::true_type> {
        return {};
    }

    template < class U >
    static std::false_type check(...) {
        static_assert(sizeof(U));
        return {};
    }


    static constexpr const bool value = decltype(check<T>(std::declval<T>()))::value;
};


enum log_level
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

class log_message
{
public:
    template < size_t N >
    log_message(log_level level, const char (&cstr)[N]) : level(level), data(cstr) {}

    template < size_t N, class... TArgs >
    log_message(log_level level, const char (&cstr)[N], TArgs&&... args) :
        level(level), data(fmt::format(cstr, std::forward< TArgs >(args)...)) {}

    log_message(log_level level, std::string str) : level(level), data(std::move(str)) {}

    log_message(const log_message&)     = default;
    log_message(log_message&&) noexcept = default;

    log_message& operator=(const log_message&) = default;
    log_message& operator=(log_message&&) noexcept = default;

    log_level    get_log_level() const noexcept {
        return level;
    }

    const char* get_msg() const {
        // Overly complicated accessor function
        const char* msg_ptr = nullptr;

        std::visit(
            [&msg_ptr](auto&& v) {
                using T = std::decay_t< decltype(v) >;
                if constexpr ( std::is_same_v< T, const char* > ) {
                    msg_ptr = v;
                } else if constexpr ( std::is_same_v< T, std::string > ) {
                    msg_ptr = v.c_str();
                }
            },
            data);

        return msg_ptr;
    }

    static const char* log_level_str(log_level level) {
        switch ( level ) {
            case LOG_DEBUG:
                return "debug";
            case LOG_INFO:
                return "info";
            case LOG_WARN:
                return "warn";
            case LOG_ERROR:
                return "error";
            default:
                throw std::invalid_argument("unknown log level");
        }
    }

private:
    log_level                                level;

    std::variant< const char*, std::string > data;
};

void _log(log_message msg);

template < class... TArgs >
void log(TArgs&&... args) {
    _log(log_message(std::forward< TArgs >(args)...));
}


template < class T, class A >
constexpr auto align_to_next_multiple(T value, A alignment) {
    static_assert(std::is_unsigned_v<T> && std::is_unsigned_v<A>, "types must be unsigned");

    auto remainder = value % alignment;

    if(remainder) {
        value += (alignment - remainder);
    }

    return value;
}

template < class TContainer, class TIterator >
void as_unique(TContainer& container, TIterator it_first, TIterator it_last) {
    static_assert(std::is_copy_constructible_v<typename TContainer::value_type>, "Assigned value must be copy constructible");

    for(TIterator it = it_first; it != it_last; ++it) {
        if(std::find(container.begin(), container.end(), *it) == container.end()) {
            std::back_inserter(container) = *it;
        }
    }
}

namespace detail
{
template < class T, size_t I, size_t N >
struct _carr_to_stdarr_helper
{
    template < class Tf, class... TArgs >
    static std::array< T, N > build(Tf (&ca)[N], TArgs&&... args) {
        return _carr_to_stdarr_helper< T, I + 1, N >::build(ca, std::forward< TArgs >(args)..., ca[I]);
    }
};

template < class T, size_t N >
struct _carr_to_stdarr_helper< T, N, N >
{
    template < class Tf, class... TArgs >
    static std::array< T, N > build(Tf (&ca)[N], TArgs&&... args) {
        return std::array< T, N > {std::forward< TArgs >(args)...};
    }
};
}

template < class T, size_t N >
std::array< std::remove_cv_t<T>, N> make_array(T (&c_array)[N]) {
    using plain_type = std::remove_cv_t<T>;

    return detail::_carr_to_stdarr_helper<plain_type, 0, N>::build(c_array);
}

template < class T >
struct factory_element
{
    using type = T;

    explicit factory_element(const char* name) : name(name) {}

    template < class... TCtorArgs >
    T construct(TCtorArgs&&... ctor_args) {
        return {std::forward< TCtorArgs >(ctor_args)...};
    }

    template < class TTarget, class... TCtorArgs >
    auto construct_and_assign(TTarget& target, TCtorArgs&&... ctor_args)
        // This should prevent assigning types where slicing occurs during assignment
        -> std::enable_if_t< std::is_trivially_assignable_v< TTarget, T > && (sizeof(T) == sizeof(TTarget)) > {

        target = T(std::forward< TCtorArgs >(ctor_args)...);
    }

    template < class TTarget, class... TCtorArgs >
    void construct_and_assign(std::unique_ptr< TTarget >& target, TCtorArgs&&... ctor_args) {
        target = std::make_unique< T >(std::forward< TCtorArgs >(ctor_args)...);
    }

    template < class TTarget, class... TCtorArgs >
    void construct_and_assign(std::shared_ptr< TTarget >& target, TCtorArgs&&... ctor_args) {
        target = std::make_shared< T >(std::forward< TCtorArgs >(ctor_args)...);
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

struct seq
{
    using value_type = size_t;

    struct iterator
    {
        value_type v;

        constexpr iterator(value_type v) : v(v) {}

        __inline value_type operator* () const noexcept {
            return v;
        }

        __inline iterator& operator++ () noexcept {
            ++v;

            return *this;
        }

        __inline bool operator == (const iterator& other) const noexcept {
            return v == other.v;
        }

        __inline bool operator != (const iterator& other) const noexcept {
            return v != other.v;
        }
    };

    value_type _start;
    value_type _end;

    constexpr seq(value_type start, value_type end) : _start(start), _end(end) {}

    constexpr iterator begin() const noexcept {return {_start};}
    constexpr iterator end() const noexcept {return {_end};}
};

std::string load_file_as_string(const std::filesystem::path& file_path);