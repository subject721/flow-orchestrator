#include <dpdk/dpdk_common.hpp>

#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_errno.h>


using namespace std;


int lcore_thread::lcore_thread_launch_trampoline(void* arg) {
    lcore_thread* t               = reinterpret_cast< lcore_thread* >(arg);

    uint32_t      actual_lcore_id = rte_lcore_id();

    if ( t->lcore_id != actual_lcore_id ) {

        return -1;
    }

    try {
        if ( t->func ) {
            {
                std::lock_guard< std::mutex > guard(t->lock);

                t->running = true;
            }

            t->func();

            {
                std::lock_guard< std::mutex > guard(t->lock);

                t->running = false;

                std::atomic_thread_fence(std::memory_order_seq_cst);

                t->event.notify_all();
            }
        }
    } catch ( ... ) { return -1; }

    return 0;
}

lcore_thread::~lcore_thread() {}

void lcore_thread::join() {
    std::unique_lock< std::mutex > lk(lock);

    event.wait(lk, [this]() { return !running; });
}

bool lcore_thread::is_joinable() {
    return running;
}

void lcore_thread::try_launch() {

    if ( 0 != rte_eal_remote_launch(lcore_thread_launch_trampoline, this, lcore_id) ) {
        throw std::runtime_error(fmt::format("could not launch function on lcore {}", lcore_id));
    }
}

dpdk_mempool::dpdk_mempool(uint32_t num_elements, uint16_t cache_size, uint16_t data_size, uint16_t private_size) {
    rte_mempool* new_pool_ptr =
        rte_pktmbuf_pool_create("my_pkt_pool", num_elements, cache_size, private_size, data_size, 0);

    if ( !new_pool_ptr ) {
        throw std::runtime_error(
            fmt::format("could not create packet memory buffer (err {})", rte_strerror(rte_errno)));
    }

    mempool = std::unique_ptr< rte_mempool, mempool_deleter >(new_pool_ptr);
}

uint32_t dpdk_mempool::get_capacity() {
    return mempool->size;
}

uint32_t dpdk_mempool::get_num_allocated() {
    return rte_mempool_in_use_count(mempool.get());
}

int dpdk_mempool::bulk_alloc(rte_mbuf** mbufs, uint16_t count) {
    return rte_pktmbuf_alloc_bulk(mempool.get(), mbufs, count);
}

int dpdk_mempool::bulk_alloc(mbuf_vec_base& mbuf_vec, uint16_t count) {
    if ( count > mbuf_vec.num_free_tail() ) {
        count = mbuf_vec.num_free_tail();
    }

    int rc = bulk_alloc(mbuf_vec.data(), count);

    if ( !rc ) {
        mbuf_vec.set_size(count);
    }

    return rc;
}

void dpdk_mempool::bulk_free(rte_mbuf** mbufs, uint16_t count) {
    for ( uint16_t index = 0; index < count; ++index ) { rte_pktmbuf_free(mbufs[index]); }
}

mbuf_ring::mbuf_ring(const std::string& name, int socket_id) : name(name), socket_id(socket_id) {}

mbuf_ring::mbuf_ring(const std::string& name, int socket_id, size_t capacity) : mbuf_ring(name, socket_id) {
    init(capacity);
}

void mbuf_ring::init(size_t capacity) {
    if ( ring ) {
        throw std::runtime_error("ring is already initialized");
    }

    auto* new_ring_ptr = rte_ring_create(name.c_str(), capacity, socket_id, RING_F_SP_ENQ | RING_F_SC_DEQ);

    if ( !new_ring_ptr ) {
        int error_code = rte_errno;

        throw std::runtime_error(
            fmt::format("could not create sp/sc ring with capacity of {}: {}", capacity, rte_strerror(error_code)));
    }

    ring = std::unique_ptr< rte_ring, rte_ring_deleter >(new_ring_ptr);
}

size_t mbuf_ring::get_capacity() const {
    if ( ring ) {
        return (size_t) rte_ring_get_capacity(ring.get());
    } else {
        return 0;
    }
}


void dpdk_eal_init(std::vector< std::string > flags) {
    std::vector< std::string > flag_list = std::move(flags);

    std::vector< char* >       pointer_list;

    for ( auto& flag : flag_list ) { pointer_list.push_back(flag.data()); }

    auto rc = rte_eal_init((int) pointer_list.size(), pointer_list.data());

    if ( 0 > rc ) {
        throw std::runtime_error(fmt::format("could not init dpdk runtime: {}", rte_strerror(rc)));
    }
}