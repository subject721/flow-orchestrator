#pragma once

#include "common/common.hpp"

#include "dpdk/dpdk_ethdev.hpp"

#include "flow_base.hpp"

class eth_endpoint : public flow_endpoint_base
{
public:
    eth_endpoint(std::string name, std::shared_ptr< dpdk_mempool > mempool, std::unique_ptr< dpdk_ethdev > eth_dev);

    ~eth_endpoint() override;

    uint16_t rx_burst(mbuf_vec_base& mbuf_vec) override;

    uint16_t tx_burst(mbuf_vec_base& mbuf_vec) override;

    /**
     * @brief Detaches the ethernet device instance from this endpoint node.
     * You must take care to only call this function while no calls to rx_burst or tx_burst are made anymore.
     * There is no safety mechanism that checks if the ethernet device instance is valid
     * @return
     */
    std::unique_ptr< dpdk_ethdev > detach_eth_dev();

protected:
    __inline dpdk_ethdev* get_ethdev() {
        return eth_dev.get();
    }

private:
    std::unique_ptr< dpdk_ethdev > eth_dev;
};
