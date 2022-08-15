/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <app_config.hpp>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include "common/common.hpp"

#include <cpptoml.h>
#include <fmt/core.h>

app_config::app_config() noexcept :
    primary_pkt_allocator_capacity(4096, "packet_allocator_capacity", min_max_limits< size_t >(0, 65536)),
    primary_pkt_allocator_cache_size(64, "packet_allocator_cache_size", min_max_limits< size_t >(0, 256)),
    flowtable_capacity(8192, "flowtable_capacity", min_max_limits< size_t >(0, 65536)),
    telemetry_bind_addr("127.0.0.1", "telemetry_bind_addr"),
    telemetry_bind_port(8123, "telemetry_bind_port"),
    telemetry_update_interval_ms(250, "telemetry_update_interval_ms", min_max_limits< uint32_t >(50, 50000)) {

    dataplane_config_params.push_back(std::ref(primary_pkt_allocator_capacity));
    dataplane_config_params.push_back(std::ref(primary_pkt_allocator_cache_size));
    dataplane_config_params.push_back(std::ref(flowtable_capacity));

    telemetry_config_params.push_back(std::ref(telemetry_bind_addr));
    telemetry_config_params.push_back(std::ref(telemetry_bind_port));
    telemetry_config_params.push_back(std::ref(telemetry_update_interval_ms));
}

template < class T, class L >
static void try_load_cfg_value(config_param< T, L >& param, std::shared_ptr< cpptoml::table >& table) {
    auto value = table->get_as< typename config_param< T, L >::value_type >(param.get_name());

    if ( value ) {
        param.set(*value);

        log(LOG_DEBUG, "config value {} set to {}", param.get_name(), param.to_string());
    }
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
        try_load_cfg_value(primary_pkt_allocator_capacity, dataplane_table);
        try_load_cfg_value(primary_pkt_allocator_cache_size, dataplane_table);
        try_load_cfg_value(flowtable_capacity, dataplane_table);
    }

    auto telemetry_table = root_table->get_table("telemetry");

    if ( telemetry_table ) {
        try_load_cfg_value(telemetry_bind_addr, telemetry_table);
        try_load_cfg_value(telemetry_bind_port, telemetry_table);
        try_load_cfg_value(telemetry_update_interval_ms, telemetry_table);
    }
}

std::vector< std::string > app_config::get_all_param_names() const {
    std::vector< std::string > names;

    for ( const auto& param : dataplane_config_params ) {
        names.push_back(
            fmt::format("{}::{}  (default {})", "dataplane", param.get().get_name(), param.get().to_string()));
    }

    for ( const auto& param : telemetry_config_params ) {
        names.push_back(
            fmt::format("{}::{}  (default {})", "telemetry", param.get().get_name(), param.get().to_string()));
    }

    return names;
}
