/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/file_utils.hpp>

#include <fstream>

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

struct filesystem_watcher::private_data
{

};

filesystem_watcher::filesystem_watcher() {

}

filesystem_watcher::~filesystem_watcher() {

}

fdescriptor::fdtype filesystem_watcher::get_fd() const {
    return fdescriptor::INVALID_FD;
}

bool filesystem_watcher::wait(uint32_t fd_op_flags, uint32_t timeout_ms) {
    return false;
}
