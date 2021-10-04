#include <common/common.hpp>
#include <dpdk/dpdk_common.hpp>


int main(int argc, char** argv) {

    try {
        dpdk_eal_init({"--no-shconf", "--in-memory"});
    } catch(const std::exception& e) {
        log(LOG_LEVEL_ERROR, format("could not init dpdk eal: %1%") % e.what());

        return 1;
    }

    try {
        mbuf_ring ring("my_ring", 0, 512);

        dpdk_mempool mempool(512, 0, 1024, 32);

        static_mbuf_vec<32> mbuf_vec;

        static_mbuf_vec<32> mbuf_vec_out;

        uint16_t burst_size = 16;

        log(LOG_LEVEL_INFO, format("mempool alloc stats after init: %1%/%2%") % mempool.get_num_allocated() % mempool.get_capacity());

        if(mempool.bulk_alloc(mbuf_vec, burst_size)) {
            throw std::runtime_error("could not alloc mbufs");
        }

        log(LOG_LEVEL_INFO, format("mempool alloc stats after alloc: %1%/%2%") % mempool.get_num_allocated() % mempool.get_capacity());

        if(ring.enqueue(mbuf_vec) != burst_size) {
            throw std::runtime_error("queuing failed");
        }

        log(LOG_LEVEL_INFO, format("mempool alloc stats after queue: %1%/%2%") % mempool.get_num_allocated() % mempool.get_capacity());

        if(ring.dequeue(mbuf_vec_out) != burst_size) {
            throw std::runtime_error("dequeuing failed");
        }

        log(LOG_LEVEL_INFO, format("mempool alloc stats after dequeue: %1%/%2%") % mempool.get_num_allocated() % mempool.get_capacity());

        if(mbuf_vec_out.size() != burst_size) {
            throw std::runtime_error("mbuf vec wrong size");
        }

        mbuf_vec_out.free();

        log(LOG_LEVEL_INFO, format("mempool alloc stats after free: %1%/%2%") % mempool.get_num_allocated() % mempool.get_capacity());

    } catch(const std::exception& e) {
        log(LOG_LEVEL_ERROR, format("error while testing ring: %1%") % e.what());
    }

    return 0;
}