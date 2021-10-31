/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/common.hpp>

#include <iostream>
#include <ios>

#include <boost/date_time/posix_time/posix_time.hpp>

static const char* NO_COLOR     = "\x1b[0m";
static const char* RED_COLOR    = "\x1b[0;31m";
static const char* GREEN_COLOR  = "\x1b[0;32m";
static const char* ORANGE_COLOR = "\x1b[0;33m";

constexpr const fdescriptor::fdtype fdescriptor::INVALID_FD;

class color_inserter : noncopyable
{
public:
    color_inserter(std::ostream& stream, const char* str) : stream(stream) {
        stream << str;
    }

    ~color_inserter() {
        stream << NO_COLOR;
    }

private:
    std::ostream& stream;
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
