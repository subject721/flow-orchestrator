#pragma once

#include "common/common.hpp"
#include "dpdk/dpdk_common.hpp"

#include <nlohmann/json.hpp>

#include <list>

using json = nlohmann::json;


class telemetry_distributor;

template < class T >
struct atomic_storage_adapter
{
    using type = typename std::atomic<T>;

    type data;

    template < class TV >
    __inline void set(TV&& new_value) {
        data.store(std::forward<TV>(new_value), std::memory_order_release);
    }

    __inline T get() const noexcept {
        return data.load(std::memory_order_acquire);
    }
};

template < class T >
struct trivial_storage_adapter
{
    using type = T;

    type data;

    template < class TV >
    __inline void set(TV&& new_value) {
        data = std::forward<TV>(new_value);
    }

    __inline T get() const noexcept {
        return data;
    }
};

class metric_base : noncopyable
{
public:

    explicit metric_base(std::string name) : name(std::move(name)), owning_distributor(nullptr) {

    }

    virtual ~metric_base();

    const std::string& get_name() const noexcept {
        return name;
    }


protected:

    void updated();

    virtual void serialize(json& v) = 0;

private:

    std::string name;

    telemetry_distributor* owning_distributor;

    friend class telemetry_distributor;
    friend class metric_group;
};

template < class T, class TStorageAdapter = atomic_storage_adapter<T> >
class scalar_metric : public metric_base
{
public:
    using value_type = T;

    using storage_adapter = TStorageAdapter;

    explicit scalar_metric(std::string name) :
        metric_base(std::move(name)),
        value() {

    }

    template < class TV >
    void set(TV&& new_value) {
        value.set(std::forward<TV>(new_value));

        updated();
    }

    value_type get() const noexcept {
        return value.get();
    }

private:

    void serialize(json& v) override {
        v = get();
    }

    storage_adapter value;
};

template < class T, class TStorageAdapter = trivial_storage_adapter<T> >
class per_lcore_metric : public metric_base
{
public:
    using value_type = T;

    using storage_adapter = TStorageAdapter;

    explicit per_lcore_metric(std::string name) :
        metric_base(std::move(name)) {

    }

    ~per_lcore_metric() override = default;

    using metric_base::get_name;


    template < class TV >
    void set(TV&& new_value) {
        auto lid = rte_lcore_id();

        if( lid == LCORE_ID_ANY ) {
            non_lcore_data.set(std::forward<TV>(new_value));
        } else {
            per_lcore_data[lid].set(std::forward<TV>(new_value));
        }
    }

    value_type get() const noexcept {
        auto lid = rte_lcore_id();

        if( lid == LCORE_ID_ANY ) {
            return non_lcore_data.get();
        } else {
            return per_lcore_data[lid].get();
        }
    }

    value_type get(unsigned int lid) const noexcept {

        if( lid == LCORE_ID_ANY ) {
            return non_lcore_data.get();
        } else {
            return per_lcore_data[lid].get();
        }
    }

private:
    void serialize(json& v) override {
        v = json::array();

        v.push_back(get(LCORE_ID_ANY));

        for(auto& d : per_lcore_data) {
            v.push_back(d.get());
        }
    }

    std::array<storage_adapter, RTE_MAX_LCORE> per_lcore_data;

    storage_adapter  non_lcore_data;
};

class metric_group : public metric_base
{
public:
    explicit metric_group(std::string name) :
        metric_base(std::move(name)) {
    }

    ~metric_group() override = default;

    void add_metric(metric_base& m);

    void remove_metric(metric_base& m);

     void serialize(json& v) override;

private:
    std::mutex lk;

    std::list<std::reference_wrapper<metric_base>> child_metrics;
};

class telemetry_distributor : noncopyable
{
public:

    telemetry_distributor(const std::string& endpoint_addr);

    ~telemetry_distributor();

    void add_metric(metric_base& m);

    void remove_metric(metric_base& m);

    void do_update();

private:
    struct private_data;

    std::unique_ptr<private_data> pdata;


};