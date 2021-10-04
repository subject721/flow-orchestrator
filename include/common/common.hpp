#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctype.h>

#include <mutex>
#include <memory>
#include <atomic>
#include <variant>
#include <filesystem>
#include <limits>

#include <config.h>

#include <boost/format.hpp>

using format = boost::format;

class noncopyable
{
protected:
    noncopyable()                   = default;

    noncopyable(const noncopyable&) = delete;

    void operator=(const noncopyable&) = delete;
};

enum log_level
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
};

class log_message
{
public:
    template < size_t N >
    log_message(log_level level, const char (&cstr)[N]) : level(level), data(cstr) {}

    log_message(log_level level, std::string str) : level(level), data(std::move(str)) {}

    log_message(log_level level, const format& fmt) : log_message(level, fmt.str()) {}

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
            case LOG_LEVEL_DEBUG:
                return "debug";
            case LOG_LEVEL_INFO:
                return "info";
            case LOG_LEVEL_WARN:
                return "warn";
            case LOG_LEVEL_ERROR:
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


std::string load_file_as_string(const std::filesystem::path& file_path);