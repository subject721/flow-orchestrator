/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/common.hpp>
#include <dpdk/dpdk_common.hpp>

#include <flow_executor.hpp>


class executor_test
{
public:
    executor_test() : executor(*this) {}

    void run_test(const std::vector< lcore_info >& available_lcores) {
        try {
            executor.setup({0, 0}, 1, available_lcores);

            executor.start(&executor_test::run_endpoint_tasks, &executor_test::run_distributor_tasks);

            rte_delay_us_sleep(10000);

            executor.stop();
        } catch ( const std::exception& e ) { log(LOG_ERROR, "test failed: {}", e.what()); }
    }


private:
    void run_endpoint_tasks(const std::vector< size_t >& indicies) {
        for ( size_t index : indicies ) {
            log(LOG_INFO, "Running endpoint task: {} on lcore {}", index, rte_lcore_id());
            rte_delay_us_sleep(200);
        }
    }

    void run_distributor_tasks(const std::vector< size_t >& indicies) {
        for ( size_t index : indicies ) {
            log(LOG_INFO, "Running distributor task: {} on lcore {}", index, rte_lcore_id());
            rte_delay_us_sleep(200);
        }
    }

    flow_executor< reduced_core_policy, executor_test > executor;
};

int main(int argc, char** argv) {

    try {
        dpdk_eal_init({"--no-shconf", "--in-memory", "-l", "1,2,3,4"});
    } catch ( const std::exception& e ) {
        log(LOG_ERROR, "could not init dpdk eal: {}", e.what());

        return 1;
    }

    executor_test test;

    test.run_test(lcore_info::get_available_worker_lcores());

    try {
        mbuf_ring ring("my_ring", 0, 512);

        dpdk_mempool mempool(512, 0, 1024, 32);

        static_mbuf_vec< 32 > mbuf_vec;

        static_mbuf_vec< 32 > mbuf_vec_out;

        uint16_t burst_size = 16;

        log(LOG_INFO, "mempool alloc stats after init: {}/{}", mempool.get_num_allocated(), mempool.get_capacity());

        if ( mempool.bulk_alloc(mbuf_vec, burst_size) ) {
            throw std::runtime_error("could not alloc mbufs");
        }

        log(LOG_INFO, "mempool alloc stats after alloc: {}/{}", mempool.get_num_allocated(), mempool.get_capacity());

        if ( ring.enqueue(mbuf_vec) != burst_size ) {
            throw std::runtime_error("queuing failed");
        }

        log(LOG_INFO, "mempool alloc stats after queue: {}/{}", mempool.get_num_allocated(), mempool.get_capacity());

        if ( ring.dequeue(mbuf_vec_out) != burst_size ) {
            throw std::runtime_error("dequeuing failed");
        }

        log(LOG_INFO, "mempool alloc stats after dequeue: {}/{}", mempool.get_num_allocated(), mempool.get_capacity());

        if ( mbuf_vec_out.size() != burst_size ) {
            throw std::runtime_error("mbuf vec wrong size");
        }

        mbuf_vec_out.free();

        log(LOG_INFO, "mempool alloc stats after free: {}/{}", mempool.get_num_allocated(), mempool.get_capacity());

    } catch ( const std::exception& e ) { log(LOG_ERROR, "error while testing ring: {}", e.what()); }

    return 0;
}