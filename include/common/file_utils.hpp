/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common.hpp"

#include <filesystem>


std::string load_file_as_string(const std::filesystem::path& file_path);


class filesystem_watch
{
public:

private:
    std::filesystem::path path;
};


class filesystem_watcher : noncopyable, public virtual fdescriptor
{
public:
    filesystem_watcher();

    ~filesystem_watcher() override;

    fdescriptor::fdtype get_fd() const override;

    bool wait(uint32_t fd_op_flags, uint32_t timeout_ms) override;


private:
    struct private_data;

    std::unique_ptr<private_data> pdata;

};