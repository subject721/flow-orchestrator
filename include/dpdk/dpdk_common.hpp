/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#pragma once

#include <common/common.hpp>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_lcore.h>
#include <rte_ring.h>

#include <functional>

#include <condition_variable>
#include <array>


class mbuf_vec_base;

class lcore_info : public std::pair< uint32_t, int >
{
public:
    lcore_info() : std::pair< uint32_t, int >(LCORE_ID_ANY, SOCKET_ID_ANY) {}

    lcore_info(uint32_t lcore_id, int socket_id) : std::pair< uint32_t, int >(lcore_id, socket_id) {}

    uint32_t get_lcore_id() const noexcept {
        return first;
    }

    int get_socket_id() const noexcept {
        return second;
    }

    static lcore_info from_lcore_id(uint32_t lcore_id);

    static std::vector< lcore_info > get_available_worker_lcores();

    static lcore_info get_main_lcore();
};


class lcore_thread : noncopyable
{
public:
    template < class TFunc >
    lcore_thread(uint32_t lcore_id, TFunc&& func) :
        lcore_id(lcore_id), func(std::forward< TFunc >(func)), running(false) {

        try_launch();
    }

    ~lcore_thread();

    void join();

    bool is_joinable();

    uint32_t get_lcore_id() const noexcept {
        return lcore_id;
    }

    static uint32_t get_current_lcore() {
        return rte_lcore_id();
    }

private:
    void try_launch();

    static int lcore_thread_launch_trampoline(void* arg);

    uint32_t lcore_id;

    std::function< void() > func;

    std::atomic_bool running;

    std::mutex lock;

    std::condition_variable event;
};


struct mempool_deleter
{
    void operator()(rte_mempool* mempool) {
        if ( !mempool )
            return;

        rte_mempool_free(mempool);
    }
};


class dpdk_mempool : noncopyable
{
public:
    dpdk_mempool(uint32_t num_elements, uint16_t cache_size, uint16_t data_size, uint16_t private_size);

    uint32_t get_capacity();

    uint32_t get_num_allocated();

    int bulk_alloc(rte_mbuf** mbufs, uint16_t count);

    int bulk_alloc(mbuf_vec_base& mbuf_vec, uint16_t count);

    static void bulk_free(rte_mbuf** mbufs, uint16_t count);

    rte_mempool* get_native() {
        return mempool.get();
    }

private:
    std::unique_ptr< rte_mempool, mempool_deleter > mempool;
};


class mbuf_vec_base
{
public:
    mbuf_vec_base() : mbufs(nullptr), head_offset(0), tail_offset(0), max_num_mbufs(0) {}


    __always_inline rte_mbuf** base() noexcept {
        return mbufs;
    }

    __always_inline rte_mbuf** begin() noexcept {
        return mbufs + head_offset;
    }

    __always_inline rte_mbuf** end() noexcept {
        return mbufs + tail_offset;
    }

    __always_inline uint16_t size() const noexcept {
        return tail_offset - head_offset;
    }

    __always_inline uint16_t num_free_tail() const noexcept {
        return max_num_mbufs - tail_offset;
    }

    __always_inline uint16_t capacity() const noexcept {
        return max_num_mbufs;
    }

    __inline void free() noexcept {
        dpdk_mempool::bulk_free(begin(), size());

        head_offset = 0;
        tail_offset = 0;
    }

    __inline void consume() noexcept {

        head_offset = 0;
        tail_offset = 0;
    }

    __inline void free_front(uint16_t num) noexcept {
        if ( unlikely(num > size()) ) {
            num = size();
        }

        dpdk_mempool::bulk_free(begin(), num);

        head_offset += num;
    }

    __inline void consume_front(uint16_t num) noexcept {
        if ( unlikely(num > size()) ) {
            num = size();
        }

        head_offset += num;
    }

    __inline void free_back(uint16_t num) noexcept {
        if ( unlikely(num > size()) ) {
            num = size();
        }

        tail_offset -= num;

        dpdk_mempool::bulk_free(mbufs + tail_offset, num);
    }

    __inline void consume_back(uint16_t num) noexcept {
        if ( unlikely(num > size()) ) {
            num = size();
        }

        tail_offset -= num;
    }

    __inline void clear_packet(uint16_t idx) noexcept {
        begin()[idx] = nullptr;
    }

    __inline uint16_t grow_tail(uint16_t num) noexcept {
        if(num > num_free_tail()) {
            num = num_free_tail();
        }

        tail_offset += num;

        return num;
    }

    __inline void set_size(uint16_t num) noexcept {
        if ( unlikely((tail_offset + num) > capacity()) ) {
            num = capacity() - tail_offset;
        }

        tail_offset += num;
    }

    __inline void repack() noexcept {
        uint16_t dst_idx = 0;

        for ( uint16_t idx = head_offset; idx < tail_offset; ++idx ) {
            if ( mbufs[idx] ) {
                if ( dst_idx != idx ) {
                    mbufs[dst_idx] = mbufs[idx];
                }

                ++dst_idx;
            }
        }

        head_offset = 0;
        tail_offset = dst_idx;
    }

protected:
    void update(rte_mbuf** new_mbufs, uint16_t new_max_num_mbufs) {
        mbufs         = new_mbufs;
        head_offset   = 0;
        tail_offset   = 0;
        max_num_mbufs = new_max_num_mbufs;
    }

private:
    rte_mbuf** mbufs;

    uint16_t head_offset;

    uint16_t tail_offset;

    uint16_t max_num_mbufs;
};

template < uint16_t MAX_SIZE >
class static_mbuf_vec : public mbuf_vec_base
{
public:
    static_assert(MAX_SIZE < std::numeric_limits< uint16_t >::max(), "MAX_SIZE exceeds maximum size");

    static_mbuf_vec() : mbuf_vec_base() {
        update(mbuf_mem.data(), (uint16_t) mbuf_mem.size());
    }

    ~static_mbuf_vec() {
        mbuf_vec_base::free();
    }

    using mbuf_vec_base::capacity;
    using mbuf_vec_base::base;
    using mbuf_vec_base::free;
    using mbuf_vec_base::free_back;
    using mbuf_vec_base::free_front;
    using mbuf_vec_base::set_size;
    using mbuf_vec_base::size;

private:
    std::array< rte_mbuf*, MAX_SIZE > mbuf_mem;
};

class mbuf_vec_view
{
public:
    __inline constexpr mbuf_vec_view(rte_mbuf** mbufs, uint16_t num_mbufs) : mbufs(mbufs), num_mbufs(num_mbufs) {}

    __inline mbuf_vec_view(mbuf_vec_base& mbuf_vec) : mbufs(mbuf_vec.begin()), num_mbufs(mbuf_vec.size()) {}

    __always_inline rte_mbuf** data() noexcept {
        return mbufs;
    }

    __always_inline uint16_t size() const noexcept {
        return num_mbufs;
    }

    __inline mbuf_vec_view to_offset(uint16_t offset) noexcept {
        if ( unlikely(offset >= num_mbufs) ) {
            offset = num_mbufs;
        }

        return {mbufs, offset};
    }

    __inline mbuf_vec_view from_offset(uint16_t offset) noexcept {
        if ( unlikely(offset >= num_mbufs) ) {
            return {nullptr, 0};
        } else {
            return {mbufs + offset, (uint16_t) (num_mbufs - offset)};
        }
    }

    __inline rte_mbuf* begin() noexcept {
        return *mbufs;
    }

    __inline rte_mbuf* end() noexcept {
        return (*mbufs) + num_mbufs;
    }

private:
    rte_mbuf** mbufs;

    uint16_t num_mbufs;
};

struct rte_ring_deleter
{
    void operator()(rte_ring* ring_ptr) {
        if ( !ring_ptr )
            return;

        while ( rte_ring_count(ring_ptr) > 0 ) {
            rte_mbuf* mbuf = nullptr;

            if ( !rte_ring_dequeue(ring_ptr, (void**) &mbuf) ) {
                dpdk_mempool::bulk_free(&mbuf, 1U);
            }
        }

        rte_ring_free(ring_ptr);
    }
};

class mbuf_ring : noncopyable
{
public:
    mbuf_ring(std::string name, int socket_id);

    mbuf_ring(std::string name, int socket_id, size_t capacity);

    mbuf_ring(mbuf_ring&& other) noexcept;

    mbuf_ring& operator=(mbuf_ring&& other) noexcept;

    void init(size_t capacity);

    size_t get_capacity() const;

    __inline uint16_t enqueue(mbuf_vec_base& mbuf_vec) {
        uint16_t num = mbuf_vec.size();

        if ( num > rte_ring_free_count(ring.get()) ) {
            num = rte_ring_free_count(ring.get());
        }

        auto rc = (uint16_t) rte_ring_enqueue_bulk(
            ring.get(), reinterpret_cast< void* const* >(mbuf_vec.begin()), num, nullptr);

        mbuf_vec.consume_front(num);

        return rc;
    }

    __inline bool enqueue_single(rte_mbuf* mbuf) {
        return (rte_ring_enqueue(ring.get(), mbuf) == 0);
    }

    __inline uint16_t dequeue(mbuf_vec_base& mbuf_vec) {
        uint16_t num = mbuf_vec.num_free_tail();

        if ( num > rte_ring_count(ring.get()) ) {
            num = rte_ring_count(ring.get());
        }

        auto rc =
            (uint16_t) rte_ring_dequeue_bulk(ring.get(), reinterpret_cast< void** >(mbuf_vec.begin()), num, nullptr);

        mbuf_vec.grow_tail(rc);

        return rc;
    }

private:
    std::string name;
    int         socket_id;

    std::unique_ptr< rte_ring, rte_ring_deleter > ring;
};


void dpdk_eal_init(std::vector< std::string > flags);
