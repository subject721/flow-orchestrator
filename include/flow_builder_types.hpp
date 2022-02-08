/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "flow_base.hpp"

#include <optional>
#include <map>

enum parameter_constraint_type
{
    PARAM_STRING,
    PARAM_INTEGER,
    PARAM_NUMBER,
    PARAM_IPv4,
    PARAM_IPv6,
    PARAM_MAC,
    PARAM_FILEPATH,
    PARAM_CUSTOM
};

class parameter_info
{
public:
    parameter_info(std::string name, parameter_constraint_type constraint_type = PARAM_STRING) :
        name(std::move(name)), constraint_type(constraint_type) {}

    const std::string& get_name() const noexcept {
        return name;
    }

    parameter_constraint_type get_constraint_type() const noexcept {
        return constraint_type;
    }

private:
    std::string name;

    parameter_constraint_type constraint_type;
};

class flow_builder_node
{
public:
    flow_builder_node() {}

    flow_builder_node(std::string inst_name, std::string class_name) :
        instance_name(std::move(inst_name)), class_name(std::move(class_name)) {}

    const std::string& get_instance_name() const noexcept {
        return instance_name;
    }

    const std::string& get_class_name() const noexcept {
        return class_name;
    }

private:
    std::string instance_name;

    std::string class_name;
};

class flow_proc_builder : public flow_builder_node
{
public:
    flow_proc_builder() {}

    flow_proc_builder(std::string instance_name, std::string class_name) :
        flow_builder_node(std::move(instance_name), std::move(class_name)) {}

    void set_param(const std::string& key, const std::string& value);

    std::optional< std::string > get_param(const std::string& key) const;

    std::shared_ptr< flow_proc_builder > next(std::shared_ptr< flow_proc_builder > p);

    std::shared_ptr< flow_proc_builder > get_next_proc() const {
        return next_proc;
    }

private:
    std::map< std::string, std::string > params;

    std::shared_ptr< flow_proc_builder > next_proc;
};

class flow_endpoint_builder : public flow_builder_node
{
public:
    flow_endpoint_builder(std::string instance_name, int port_num) :
        flow_builder_node(std::move(instance_name), {}), port_num(port_num) {}

    std::shared_ptr< flow_proc_builder > add_rx_proc(std::shared_ptr< flow_proc_builder > p);

    std::shared_ptr< flow_proc_builder > add_tx_proc(std::shared_ptr< flow_proc_builder > p);

    std::shared_ptr< flow_proc_builder > get_first_rx_proc() const {
        return first_rx_proc;
    }

    std::shared_ptr< flow_proc_builder > get_first_tx_proc() const {
        return first_tx_proc;
    }

    int get_port_num() const noexcept {
        return port_num;
    }

private:
    std::shared_ptr< flow_proc_builder > first_rx_proc;
    std::shared_ptr< flow_proc_builder > first_tx_proc;

    int port_num;
};