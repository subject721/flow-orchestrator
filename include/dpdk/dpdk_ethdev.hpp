/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include <common/common.hpp>

#include "dpdk_common.hpp"

#include <rte_ethdev.h>


class dpdk_ethdev : noncopyable
{
public:
    class eth_device_info
    {
    public:
        eth_device_info(uint64_t port_id, std::string name) noexcept : port_id(port_id), name(std::move(name)) {}

        uint64_t get_port_id() const noexcept {
            return port_id;
        }

        const std::string& get_name() const noexcept {
            return name;
        }

        bool operator==(const eth_device_info& other) const noexcept {
            return port_id == other.port_id;
        }

    private:
        uint64_t port_id;

        std::string name;
    };

    explicit dpdk_ethdev(uint64_t                        port_id,
                         uint64_t                        offload_flags,
                         uint16_t                        num_rx_descriptors,
                         uint16_t                        num_tx_descriptors,
                         uint16_t                        num_rx_queues,
                         uint16_t                        num_tx_queues,
                         std::shared_ptr< dpdk_mempool > mempool);

    ~dpdk_ethdev();

    uint64_t get_port_id() const noexcept;

    void start();

    void stop();

    uint16_t rx_burst(uint16_t queue_id, rte_mbuf** mbufs, uint16_t num_mbufs);

    uint16_t tx_burst(uint16_t queue_id, rte_mbuf** mbufs, uint16_t num_mbufs);

    uint16_t tx_flush(uint16_t queue_id);

    void enable_promiscious_mode(bool state);

    rte_ether_addr get_mac_addr() const;

    static eth_device_info get_device_info(uint64_t port_id);

private:
    uint64_t port_id;

    std::shared_ptr< dpdk_mempool > mempool;

    rte_eth_dev_info local_dev_info;

    rte_eth_conf local_dev_conf;

    bool configured;

    bool started;

    bool is_up;
};

std::vector< uint64_t > get_available_ethdev_ids();