#include <common/common.hpp>

#include <iostream>
#include <ios>
#include <fstream>

#include <boost/date_time/posix_time/posix_time.hpp>

static const char* NO_COLOR     = "\x1b[0m";
static const char* RED_COLOR    = "\x1b[0;31m";
static const char* GREEN_COLOR  = "\x1b[0;32m";
static const char* ORANGE_COLOR = "\x1b[0;33m";

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

    ptime       now       = microsec_clock::local_time();

    auto&       stream    = (msg.get_log_level() >= LOG_WARN) ? std::cerr : std::cout;

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

std::string load_file_as_string(const std::filesystem::path& file_path) {
    if ( exists(file_path) ) {
        std::ifstream file_stream(file_path.c_str());

        if ( !file_stream.is_open() ) {
            throw std::runtime_error("could not open file for reading");
        }

        size_t file_size;

        file_stream.seekg(0, std::ios::end);
        file_size = file_stream.tellg();
        file_stream.seekg(0);

        std::string out;

        out.resize(file_size);

        file_stream.read(&out.front(), file_size);

        return out;
    } else {
        throw std::runtime_error("file does not exist");
    }
}
