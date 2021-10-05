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

#include <fmt/format.h>

class noncopyable
{
protected:
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


std::string load_file_as_string(const std::filesystem::path& file_path);