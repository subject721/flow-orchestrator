/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common/common.hpp"

#include "dpdk/dpdk_common.hpp"
#include "dpdk/dpdk_ethdev.hpp"

#include "flow_base.hpp"


class flow_processor : public flow_node_base
{
public:
    flow_processor(std::string name, std::shared_ptr<dpdk_mempool> mempool);

    ~flow_processor() override = default;


    virtual uint16_t process(mbuf_vec_base& mbuf_vec) = 0;

private:


};


class ingress_packet_validator : public flow_processor
{
public:
    ingress_packet_validator(std::string name, std::shared_ptr<dpdk_mempool> mempool);

    ~ingress_packet_validator() override = default;

    uint16_t process(mbuf_vec_base& mbuf_vec) override;

    void set_rx_port_id(uint16_t new_rx_port_id) {
        rx_port_id = new_rx_port_id;
    }

private:
    static bool handle_ipv4_packet(const uint8_t* ipv4_header_base, uint16_t l3_len, packet_private_info* packet_info);

    uint16_t rx_port_id;
};

class flow_classifier : public flow_processor
{
public:
    flow_classifier(std::string name, std::shared_ptr<dpdk_mempool> mempool, std::shared_ptr<flow_database> flow_database_ptr);

    ~flow_classifier() override = default;

    uint16_t process(mbuf_vec_base& mbuf_vec) override;

private:
    std::shared_ptr<flow_database> flow_database_ptr;
};