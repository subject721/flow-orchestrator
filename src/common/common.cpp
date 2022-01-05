/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/common.hpp>

#include <iostream>
#include <ios>

#include <sys/types.h>
#include <unistd.h>


#include <boost/date_time/posix_time/posix_time.hpp>

static const char* NO_COLOR     = "\x1b[0m";
static const char* RED_COLOR    = "\x1b[0;31m";
static const char* GREEN_COLOR  = "\x1b[0;32m";
static const char* ORANGE_COLOR = "\x1b[0;33m";

constexpr const fdescriptor::fdtype fdescriptor::INVALID_FD;

template < class TStream >
class color_inserter : noncopyable
{
public:
    using stream_type = TStream;

    color_inserter(stream_type& stream, const char* str) : stream(stream) {
        stream << str;
    }

    ~color_inserter() {
        stream << NO_COLOR;
    }

private:
    stream_type& stream;
};

void _log(log_message msg) {
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    ptime now = microsec_clock::local_time();

    auto& stream = (msg.get_log_level() >= LOG_WARN) ? std::cerr : std::cout;

    const char* color_str = NO_COLOR;

    if ( msg.get_log_level() == LOG_WARN ) {
        color_str = ORANGE_COLOR;
    } else if ( msg.get_log_level() == LOG_ERROR ) {
        color_str = RED_COLOR;
    } else if ( msg.get_log_level() == LOG_INFO ) {
        color_str = GREEN_COLOR;
    }

    color_inserter color_inserter(stream, color_str);

    stream << to_simple_string(now) << " [" << std::setw(5) << log_message::log_level_str(msg.get_log_level())
           << "] : " << msg.get_msg() << std::endl;
}


FILE* log_proxy::get_cfile() {
    cookie_io_functions_t log_proxy_funcs = {.read  = log_proxy::read_proxy,
                                             .write = log_proxy::write_proxy,
                                             .seek  = log_proxy::seek_proxy,
                                             .close = log_proxy::close_proxy};

    log_proxy* p = new log_proxy;

    return ::fopencookie(p, "w+", log_proxy_funcs);
}

ssize_t log_proxy::read_proxy(void* p, char* buf, size_t size) {
    return 0;
}
ssize_t log_proxy::write_proxy(void* p, const char* buf, size_t size) {

    auto* proxy = reinterpret_cast< log_proxy* >(p);

    size_t data_len = strnlen(buf, size - 1);

    if ( proxy->linebuffer.size() < (data_len + 1) ) {
        proxy->linebuffer.resize(data_len + 1);
    }

    const char* cursor      = buf;
    const char* last_cursor = buf;
    const char* end         = buf + data_len;

    for ( cursor = std::find(cursor, end, '\n'); cursor < buf; cursor = std::find(cursor + 1, end, '\n') ) {
        proxy->linebuffer.insert(proxy->linebuffer.begin(), last_cursor, cursor);
        long insertion_index = cursor - last_cursor;
        proxy->linebuffer.insert(proxy->linebuffer.begin() + insertion_index, '\0');

        log(LOG_INFO, "<DPDK> {}", proxy->linebuffer.data());

        last_cursor = cursor;
    }

    if ( last_cursor < end ) {
        proxy->linebuffer.insert(proxy->linebuffer.begin(), last_cursor, end);
        long insertion_index = end - last_cursor;
        proxy->linebuffer.insert(proxy->linebuffer.begin() + insertion_index, '\0');

        log(LOG_INFO, "<DPDK> {}", proxy->linebuffer.data());
    }

    return (ssize_t) data_len;
}
int log_proxy::seek_proxy(void* p, off64_t* offset, int whence) {
    return -1;
}
int log_proxy::close_proxy(void* p) {
    auto* proxy = reinterpret_cast< log_proxy* >(p);

    delete proxy;

    return 0;
}