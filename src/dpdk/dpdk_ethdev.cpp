/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <dpdk/dpdk_ethdev.hpp>

#include <rte_ethdev.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_debug.h>
#include <rte_errno.h>

#include <cstring>

#include <boost/format.hpp>


dpdk_ethdev::dpdk_ethdev(uint64_t                        port_id,
                         uint64_t                        offload_flags,
                         uint16_t                        num_rx_descriptors,
                         uint16_t                        num_tx_descriptors,
                         uint16_t                        num_rx_queues,
                         uint16_t                        num_tx_queues,
                         std::shared_ptr< dpdk_mempool > mempool) :
    mempool(std::move(mempool)), configured(false), started(false), is_up(false) {

    std::memset(&local_dev_info, 0, sizeof(local_dev_info));

    std::memset(&local_dev_conf, 0, sizeof(local_dev_conf));

    int status = rte_eth_dev_info_get(port_id, &local_dev_info);

    if ( status ) {
        throw std::runtime_error(
            fmt::format("could not get eth device info for port {}: {}}", port_id, rte_strerror(status)));
    }

    this->port_id = port_id;

    if ( local_dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE )
        local_dev_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    if ( (offload_flags & DEV_TX_OFFLOAD_IPV4_CKSUM) && (local_dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) ) {
        local_dev_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
    }

    if ( (offload_flags & DEV_TX_OFFLOAD_UDP_CKSUM) && (local_dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) ) {
        local_dev_conf.txmode.offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;
    }

    if ( (offload_flags & DEV_TX_OFFLOAD_TCP_CKSUM) && (local_dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) ) {
        local_dev_conf.txmode.offloads |= DEV_TX_OFFLOAD_TCP_CKSUM;
    }

    status = rte_eth_dev_configure(port_id, num_rx_queues, num_tx_queues, &local_dev_conf);

    if ( status < 0 ) {
        throw std::runtime_error(fmt::format("could not configure eth device {}: {}", port_id, rte_strerror(status)));
    }

    status = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &num_rx_descriptors, &num_tx_descriptors);

    if ( status < 0 ) {
        throw std::runtime_error(fmt::format(
            "could not adjust number of rx/tx descriptors for device {}: {}", port_id, rte_strerror(status)));
    }

    rte_eth_txconf tx_conf = local_dev_info.default_txconf;
    rte_eth_rxconf rx_conf = local_dev_info.default_rxconf;

    tx_conf.offloads = local_dev_conf.txmode.offloads;
    rx_conf.offloads = local_dev_conf.rxmode.offloads;

    for ( uint16_t queue_index = 0; queue_index < num_tx_queues; ++queue_index ) {
        status =
            rte_eth_tx_queue_setup(port_id, queue_index, num_tx_descriptors, rte_eth_dev_socket_id(port_id), &tx_conf);

        if ( status < 0 ) {
            throw std::runtime_error(
                fmt::format("could not config tx queue {} device {}: {}}", queue_index, port_id, rte_strerror(status)));
        }
    }

    for ( uint16_t queue_index = 0; queue_index < num_rx_queues; ++queue_index ) {
        status = rte_eth_rx_queue_setup(port_id,
                                        queue_index,
                                        num_rx_descriptors,
                                        rte_eth_dev_socket_id(port_id),
                                        &rx_conf,
                                        this->mempool->get_native());

        if ( status < 0 ) {
            throw std::runtime_error(
                fmt::format("could not config rx queue {} device {}: {}", queue_index, port_id, rte_strerror(status)));
        }
    }

    status = rte_eth_dev_set_ptypes(port_id, RTE_PTYPE_UNKNOWN, nullptr, 0);

    if ( status < 0 ) {
        throw std::runtime_error(
            fmt::format("could not disable packet type filter for device {}: {}", port_id, rte_strerror(status)));
    }
}

dpdk_ethdev::~dpdk_ethdev() {
    if ( configured ) {
        if ( is_up ) {
            rte_eth_dev_set_link_down(port_id);

            is_up = false;
        }

        if ( started ) {
            stop();
        }

        rte_eth_dev_close(port_id);

        configured = false;
    }
}

uint64_t dpdk_ethdev::get_port_id() const noexcept {
    return port_id;
}

void dpdk_ethdev::start() {
    if ( started ) {
        throw std::runtime_error("device already started");
    }

    int status = rte_eth_dev_start(port_id);

    if ( status < 0 ) {
        throw std::runtime_error(fmt::format("could not start eth device {}: {}", port_id, rte_strerror(status)));
    }

    started = true;
}

void dpdk_ethdev::stop() {
    if ( started ) {
        int status = rte_eth_dev_stop(port_id);

        if ( status < 0 ) {
            throw std::runtime_error(fmt::format("could not stop eth device {}: {}", port_id, rte_strerror(status)));
        }

        started = false;
    }
}

uint16_t dpdk_ethdev::rx_burst(uint16_t queue_id, rte_mbuf** mbufs, uint16_t num_mbufs) {
    return rte_eth_rx_burst(port_id, queue_id, mbufs, num_mbufs);
}

uint16_t dpdk_ethdev::tx_burst(uint16_t queue_id, rte_mbuf** mbufs, uint16_t num_mbufs) {
    return rte_eth_tx_burst(port_id, queue_id, mbufs, num_mbufs);
}

uint16_t dpdk_ethdev::tx_flush(uint16_t queue_id) {
    return 0;
}

rte_ether_addr dpdk_ethdev::get_mac_addr() const {
    rte_ether_addr mac_addr {.addr_bytes = {0}};

    int status = rte_eth_macaddr_get(port_id, &mac_addr);

    if ( status < 0 ) {
        throw std::runtime_error(
            fmt::format("could not get mac address of eth device {}: {}", port_id, rte_strerror(status)));
    }

    return mac_addr;
}


dpdk_ethdev::eth_device_info dpdk_ethdev::get_device_info(uint64_t port_id) {
    rte_eth_dev_info dev_info {};

    std::memset(&dev_info, 0, sizeof(dev_info));

    int status = rte_eth_dev_info_get(port_id, &dev_info);

    if ( status ) {
        throw std::runtime_error(
            fmt::format("could not get eth device info for port {}: {}", port_id, rte_strerror(status)));
    }

    std::string dev_name(dev_info.device->name);

    return {port_id, std::move(dev_name)};
}


std::vector< uint64_t > get_available_ethdev_ids() {
    uint64_t owner_id = RTE_ETH_DEV_NO_OWNER;

    std::vector< uint64_t > port_ids;

    for ( uint64_t port_id = rte_eth_find_next_owned_by(0, owner_id); port_id < (uint64_t) RTE_MAX_ETHPORTS;
          port_id          = rte_eth_find_next_owned_by(port_id + 1, owner_id) ) {

        port_ids.push_back(port_id);
    }

    return port_ids;
}
