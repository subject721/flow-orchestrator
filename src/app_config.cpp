/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <app_config.hpp>
#include <filesystem>
#include <stdexcept>
#include "common/common.hpp"

#include <cpptoml.h>
#include <fmt/core.h>

app_config::app_config() noexcept :
    primary_pkt_allocator_capacity(4096, "packet_allocator_capacity", min_max_limits< size_t >(0, 65536)),
    primary_pkt_allocator_cache_size(64, "packet_allocator_cache_size", min_max_limits< size_t >(0, 256)),
    flowtable_capacity(8192, "flowtable_capacity", min_max_limits< size_t >(0, 65536)) {

    dataplane_config_params.push_back(std::ref(primary_pkt_allocator_capacity));
    dataplane_config_params.push_back(std::ref(primary_pkt_allocator_cache_size));
    dataplane_config_params.push_back(std::ref(flowtable_capacity));
}

void app_config::load_from_toml(const std::filesystem::path& cfg_file_path) {
    log(LOG_DEBUG, "Trying to read config file {}", cfg_file_path.generic_u8string());

    auto status = std::filesystem::status(cfg_file_path);

    if ( status.type() == std::filesystem::file_type::not_found ) {
        throw std::runtime_error(fmt::format("file {} not found", cfg_file_path.generic_u8string()));
    } else if ( status.type() == std::filesystem::file_type::none ) {
        throw std::runtime_error(fmt::format("file {} does not exist", cfg_file_path.generic_u8string()));
    } else if ( status.type() == std::filesystem::file_type::directory ) {
        throw std::runtime_error(
            fmt::format("{} is a directory but expected a file", cfg_file_path.generic_u8string()));
    } else if ( status.type() == std::filesystem::file_type::unknown ) {
        throw std::runtime_error(fmt::format("{} is of unknown type", cfg_file_path.generic_u8string()));
    }

    auto root_table = cpptoml::parse_file(cfg_file_path.u8string());

    if ( !root_table ) {
        throw std::runtime_error(fmt::format("could not read TOML file {}", cfg_file_path.generic_u8string()));
    }

    auto dataplane_table = root_table->get_table("dataplane");

    if ( dataplane_table ) {
        for ( auto& cfg_param : dataplane_config_params ) {
            auto cfg_value = dataplane_table->get_as< std::string >(cfg_param.get().name);

            if ( cfg_value ) {
                cfg_param.get().set_from_string(*cfg_value);
            }
        }
    }
}
