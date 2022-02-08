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
#include <cstdio>

#include <mutex>
#include <memory>
#include <atomic>
#include <variant>
#include <limits>
#include <algorithm>
#include <functional>

#include <config.h>

#include <fmt/format.h>

class noncopyable
{
public:
    noncopyable() = default;

    noncopyable(const noncopyable&) = delete;

    void operator=(const noncopyable&) = delete;
};

template < class T >
struct is_clonable
{
    template < class U >
    using check_impl = std::is_same< decltype(std::declval< U >().clone()), U >;

    template < class U >
    static auto check(U&& u) -> std::enable_if_t< check_impl< std::decay_t< U > >::value, std::true_type > {
        return {};
    }

    template < class U >
    static std::false_type check(...) {
        static_assert(sizeof(U));
        return {};
    }


    static constexpr const bool value = decltype(check< T >(std::declval< T >()))::value;
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

    log_level get_log_level() const noexcept {
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
    log_level level;

    std::variant< const char*, std::string > data;
};

void _log(log_message msg);

template < class... TArgs >
void log(TArgs&&... args) {
    _log(log_message(std::forward< TArgs >(args)...));
}

#ifdef DEBUG
#define DBG(FMT, ...) log(LOG_DEBUG, FMT __VA_OPT__(,) __VA_ARGS__)
#else // DEBUG
#define DBG(FMT, ...)
#endif // DEBUG


template < class T, class A >
constexpr auto align_to_next_multiple(T value, A alignment) {
    static_assert(std::is_unsigned_v< T > && std::is_unsigned_v< A >, "types must be unsigned");

    auto remainder = value % alignment;

    if ( remainder ) {
        value += (alignment - remainder);
    }

    return value;
}

template < class TContainer, class TIterator >
void as_unique(TContainer& container, TIterator it_first, TIterator it_last) {
    static_assert(std::is_copy_constructible_v< typename TContainer::value_type >,
                  "Assigned value must be copy constructible");

    for ( TIterator it = it_first; it != it_last; ++it ) {
        if ( std::find(container.begin(), container.end(), *it) == container.end() ) {
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
}  // namespace detail

template < class T, size_t N >
std::array< std::remove_cv_t< T >, N > make_array(T (&c_array)[N]) {
    using plain_type = std::remove_cv_t< T >;

    return detail::_carr_to_stdarr_helper< plain_type, 0, N >::build(c_array);
}

struct seq
{
    using value_type = size_t;

    struct iterator
    {
        value_type v;

        constexpr iterator(value_type v) : v(v) {}

        __inline value_type operator*() const noexcept {
            return v;
        }

        __inline iterator& operator++() noexcept {
            ++v;

            return *this;
        }

        __inline bool operator==(const iterator& other) const noexcept {
            return v == other.v;
        }

        __inline bool operator!=(const iterator& other) const noexcept {
            return v != other.v;
        }
    };

    value_type _start;
    value_type _end;

    constexpr seq(value_type start, value_type end) : _start(start), _end(end) {}

    constexpr iterator begin() const noexcept {
        return {_start};
    }
    constexpr iterator end() const noexcept {
        return {_end};
    }
};

enum fd_op_flag : uint32_t
{
    FD_READ,
    FD_WRITE,
    FD_ERROR
};

class fdescriptor
{
public:
    using fdtype = int;

    static constexpr const fdtype INVALID_FD = -1;

    virtual ~fdescriptor() = default;

    virtual fdtype get_fd() const = 0;

    virtual bool wait(uint32_t fd_op_flags, uint32_t timeout_ms) = 0;
};

class log_proxy : noncopyable
{
public:

    static FILE* get_cfile();

private:
    static ssize_t read_proxy(void* p, char* buf, size_t size);
    static ssize_t write_proxy(void* p, const char* buf, size_t size);
    static int seek_proxy(void* p, off64_t* offset, int whence);
    static int close_proxy(void* p);

    std::vector<char> linebuffer;
};