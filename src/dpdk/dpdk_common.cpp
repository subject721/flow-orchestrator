/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <dpdk/dpdk_common.hpp>
#include <utility>

#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_malloc.h>

using namespace std;

void mempool_deleter::operator()(rte_mempool* mempool) {
        if ( !mempool )
            return;

        rte_mempool_free(mempool);
    }

void dpdk_malloc_deleter::operator()(void* ptr) {
    rte_free(ptr);
}

void dpdk_memzone_deleter::operator()(const rte_memzone* memzone) {
    rte_memzone_free(memzone);
}

std::string lcore_info::to_string() const {
    return fmt::format("core {} on node {}", get_lcore_id(), get_socket_id());
}

lcore_info lcore_info::from_lcore_id(uint32_t lcore_id) {
    return {lcore_id, static_cast< int >(rte_lcore_to_socket_id(lcore_id))};
}

std::vector< lcore_info > lcore_info::get_available_worker_lcores() {
    std::vector< lcore_info > lcore_info_list;

    int lcore_id;

    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        lcore_info_list.push_back(from_lcore_id(lcore_id));
    }

    return lcore_info_list;
}

lcore_info lcore_info::get_main_lcore() {
    return from_lcore_id(rte_get_main_lcore());
}

int lcore_thread::lcore_thread_launch_trampoline(void* arg) {
    lcore_thread* t = reinterpret_cast< lcore_thread* >(arg);

    uint32_t actual_lcore_id = rte_lcore_id();

    if ( t->lcore_id != actual_lcore_id ) {

        return -1;
    }

    try {
        if ( t->func ) {
            t->running.store(true);

            t->func();

            {
                std::lock_guard< std::mutex > guard(t->lock);

                t->running.store(false);

                t->event.notify_all();
            }
        }
    } catch ( ... ) { return -1; }

    return 0;
}

lcore_thread::~lcore_thread() {}

void lcore_thread::join() {
    std::unique_lock< std::mutex > lk(lock);

    event.wait(lk, [this]() { return !running.load(); });
}

bool lcore_thread::is_joinable() {
    return running.load();
}

void lcore_thread::try_launch() {

    if ( 0 != rte_eal_remote_launch(lcore_thread_launch_trampoline, this, lcore_id) ) {
        throw std::runtime_error(fmt::format("could not launch function on lcore {}", lcore_id));
    }
}

dpdk_packet_mempool::dpdk_packet_mempool(uint32_t num_elements, uint16_t cache_size, uint16_t data_size, uint16_t private_size) {
    rte_mempool* new_pool_ptr =
        rte_pktmbuf_pool_create("my_pkt_pool", num_elements, cache_size, private_size, data_size, 0);

    if ( !new_pool_ptr ) {
        throw std::runtime_error(
            fmt::format("could not create packet memory buffer (err {})", rte_strerror(rte_errno)));
    }

    mempool = std::unique_ptr< rte_mempool, mempool_deleter >(new_pool_ptr);
}

uint32_t dpdk_packet_mempool::get_capacity() {
    return mempool->size;
}

uint32_t dpdk_packet_mempool::get_num_allocated() {
    return rte_mempool_in_use_count(mempool.get());
}

int dpdk_packet_mempool::bulk_alloc(rte_mbuf** mbufs, uint16_t count) {
    return rte_pktmbuf_alloc_bulk(mempool.get(), mbufs, count);
}

int dpdk_packet_mempool::bulk_alloc(mbuf_vec_base& mbuf_vec, uint16_t count) {
    if ( count > mbuf_vec.num_free_tail() ) {
        count = mbuf_vec.num_free_tail();
    }

    int rc = bulk_alloc(mbuf_vec.end(), count);

    if ( !rc ) {
        mbuf_vec.grow_tail(count);
    }

    return rc;
}

void dpdk_packet_mempool::bulk_free(rte_mbuf** mbufs, uint16_t count) {
    for ( uint16_t index = 0; index < count; ++index ) {
        rte_pktmbuf_free(mbufs[index]);
    }
}

mbuf_ring::mbuf_ring(std::string name, int socket_id) : name(std::move(name)), socket_id(socket_id) {}

mbuf_ring::mbuf_ring(std::string name, int socket_id, size_t capacity) : mbuf_ring(std::move(name), socket_id) {
    init(capacity);
}

mbuf_ring::mbuf_ring(mbuf_ring&& other) noexcept :
    name(std::move(other.name)), socket_id(other.socket_id), ring(std::move(other.ring)) {}

mbuf_ring& mbuf_ring::operator=(mbuf_ring&& other) noexcept {
    if ( this != std::addressof(other) ) {
        name      = std::move(other.name);
        socket_id = other.socket_id;
        ring      = std::move(other.ring);
    }

    return *this;
}

void mbuf_ring::init(size_t capacity) {
    if ( ring ) {
        throw std::runtime_error("ring is already initialized");
    }

    auto* new_ring_ptr = rte_ring_create(name.c_str(), capacity, socket_id, RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ);

    if ( !new_ring_ptr ) {
        int error_code = rte_errno;

        throw std::runtime_error(
            fmt::format("could not create mp/mc ring with capacity of {}: {}", capacity, rte_strerror(error_code)));
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

    std::vector< char* > pointer_list;

    for ( auto& flag : flag_list ) {
        pointer_list.push_back(flag.data());
    }

    auto rc = rte_eal_init((int) pointer_list.size(), pointer_list.data());

    if ( 0 > rc ) {
        throw std::runtime_error(fmt::format("could not init dpdk runtime: {}", rte_strerror(rc)));
    }
}