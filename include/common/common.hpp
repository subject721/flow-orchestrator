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