/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */


#include <flow_builder_types.hpp>



void flow_proc_builder::set_param(const std::string& key, const std::string& value) {
    params[key] = value;
}

std::optional< std::string > flow_proc_builder::get_param(const std::string& key) const {
    auto it = params.find(key);

    return (it != params.end()) ? it->second : std::optional< std::string > {};
}

std::shared_ptr< flow_proc_builder > flow_proc_builder::next(std::shared_ptr< flow_proc_builder > p) {
    next_proc = std::move(p);

    return next_proc;
}


std::shared_ptr< flow_proc_builder > flow_endpoint_builder::add_rx_proc(std::shared_ptr< flow_proc_builder > p) {
    if ( !first_rx_proc ) {
        first_rx_proc = std::move(p);

        return first_rx_proc;
    } else {
        std::shared_ptr< flow_proc_builder > current = first_rx_proc;

        while ( current->get_next_proc() ) {
            current = current->get_next_proc();
        }

        return current->next(std::move(p));
    }
}

std::shared_ptr< flow_proc_builder > flow_endpoint_builder::add_tx_proc(std::shared_ptr< flow_proc_builder > p) {
    if ( !first_tx_proc ) {
        first_tx_proc = std::move(p);

        return first_tx_proc;
    } else {
        std::shared_ptr< flow_proc_builder > current = first_tx_proc;

        while ( current->get_next_proc() ) {
            current = current->get_next_proc();
        }

        return current->next(std::move(p));
    }
}