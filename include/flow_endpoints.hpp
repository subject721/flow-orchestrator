/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include "common/common.hpp"

#include "dpdk/dpdk_ethdev.hpp"

#include "flow_base.hpp"


class eth_dpdk_endpoint : public flow_endpoint_base
{
public:
    eth_dpdk_endpoint(std::string                     name,
                      std::shared_ptr< dpdk_mempool > mempool,
                      std::unique_ptr< dpdk_ethdev >  eth_dev);

    ~eth_dpdk_endpoint() override;

    uint16_t rx_burst(mbuf_vec_base& mbuf_vec) override;

    uint16_t tx_burst(mbuf_vec_base& mbuf_vec) override;

    void start() override;

    void stop() override;

    /**
     * @brief Detaches the ethernet device instance from this endpoint node.
     * You must take care to only call this function while no calls to rx_burst or tx_burst are made anymore.
     * There is no safety mechanism that checks if the ethernet device instance is valid
     * @return
     */
    std::unique_ptr< dpdk_ethdev > detach_eth_dev();

#if TELEMETRY_ENABLED == 1
    void init_telemetry(telemetry_distributor& telemetry) override {
        telemetry.add_metric(metric_group);

        metric_group.add_metric(tx_packets);
        metric_group.add_metric(rx_packets);
        metric_group.add_metric(tx_bytes);
        metric_group.add_metric(rx_bytes);
    }
#endif

protected:
    __inline dpdk_ethdev* get_ethdev() {
        return eth_dev.get();
    }

private:
    std::unique_ptr< dpdk_ethdev > eth_dev;

#if TELEMETRY_ENABLED == 1
    metric_group metric_group;
    scalar_metric<uint64_t> tx_packets;
    scalar_metric<uint64_t> rx_packets;
    scalar_metric<uint64_t> tx_bytes;
    scalar_metric<uint64_t> rx_bytes;
#endif
};
