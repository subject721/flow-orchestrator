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
