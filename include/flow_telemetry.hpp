#pragma once

#include "common/common.hpp"
#include "dpdk/dpdk_common.hpp"

#include <nlohmann/json.hpp>

#include <list>

using json = nlohmann::json;


class metric_base;
class telemetry_distributor;


enum class metric_op
{
    SET,
    ADD,
    INC
};

enum class metric_unit
{
    NONE,
    PACKETS,
    BITS,
    BYTES,
    NANOSECONDS, // For now define seperate units for nano,micro,milli,... seconds. Later think about a smarter way
    MICROSECONDS,
    MILLISECONDS,
    SECONDS
};

template < class T, class U = void >
struct metric_serializer
{
    static void convert(json& j, const T& value) {
        j["type"] = "string";
        j["value"] = fmt::to_string(value);
    }
};

template <class T>
struct metric_serializer<T, std::enable_if_t<std::is_same_v<T, std::string>>>
{
    static void convert(json& j, const T& value) {
        j["type"] = "string";
        j["value"] = value;
    }
};

template <class T>
struct metric_serializer<T, std::enable_if_t<std::is_integral_v<T>>>
{
    static void convert(json& j, const T& value) {
        j["type"] = "integer";
        if constexpr (std::is_unsigned_v<T>) {
            j["value"] = (uint64_t)value;
        } else {
            j["value"] = (int64_t)value;
        }

    }
};

template <class T>
struct metric_serializer<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    static void convert(json& j, const T& value) {
        j["type"] = "number";
        j["value"] = value;
    }
};

template <class T>
struct add_aggregator
{
    template < class TStorageAdapter, size_t N >
    static T convert(std::array<TStorageAdapter, N>& per_lcore_data, TStorageAdapter& non_lcore_data) {
        T result = (T)0;

        for(auto& per_lcore_val : per_lcore_data) {
            result += per_lcore_val.get();
        }

        result += non_lcore_data.get();

        return result;
    }
};

template < class T >
struct atomic_storage_adapter
{
    using type = typename std::atomic<T>;

    type data;

    template < class TV >
    __inline void set(TV&& new_value) {
        data.store(std::forward<TV>(new_value), std::memory_order_release);
    }

    __inline void add(const T& v) {
        atomic_fetch_add(&data, v);
    }

    __inline void inc() {
        ++data;
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

    __inline void add(const T& v) {
        data += v;
    }

    __inline void inc() {
        ++data;
    }

    __inline T get() const noexcept {
        return data;
    }
};


template < class T, class U = void >
struct autoselect_adapter {
    using type = trivial_storage_adapter<T>;
};

template < class T>
struct autoselect_adapter<T, std::enable_if_t<std::is_arithmetic_v<T> && (sizeof(T) <=sizeof(uintptr_t))>>
{
    using type = atomic_storage_adapter<T>;
};

template < class T >
using autoselect_adapter_t = typename autoselect_adapter<T>::type;

class metric_base : noncopyable
{
public:

    explicit metric_base(std::string name, metric_unit unit = metric_unit::NONE) : name(std::move(name)), unit(unit), owning_distributor(nullptr) {

    }

    virtual ~metric_base();

    const std::string& get_name() const noexcept {
        return name;
    }

    metric_unit get_unit() const noexcept {
        return unit;
    }

    static const char* get_unit_str(metric_unit unit);

protected:

    void updated();

    virtual void serialize(json& v, const std::string& prefix) = 0;

private:

    std::string name;
    metric_unit unit;

    telemetry_distributor* owning_distributor;

    friend class telemetry_distributor;
    friend class metric_group;
};

template < class T, class TStorageAdapter = autoselect_adapter_t<T>, class TSerializer = metric_serializer<T> >
class scalar_metric : public metric_base
{
public:
    using value_type = T;

    using storage_adapter = TStorageAdapter;

    using serializer = TSerializer;

    explicit scalar_metric(std::string name, metric_unit unit = metric_unit::NONE) :
        metric_base(std::move(name), unit),
        value() {

    }

    template < class TV >
    __inline void set(TV&& new_value) {
        value.set(std::forward<TV>(new_value));
    }

    __inline void add(const T& v) {
        value.add(v);
    }

    __inline void inc() {
        value.inc();
    }

    value_type get() const noexcept {
        return value.get();
    }

private:

    void serialize(json& v, const std::string& prefix) override {
        json value_obj;

        serializer::convert(value_obj, get());

        v.push_back({{"label", prefix}, {"value", std::move(value_obj)}, {"unit", metric_base::get_unit_str(get_unit())}});

    }

    storage_adapter value;
};

template < class T, class TStorageAdapter = trivial_storage_adapter<T>, class TSerializer = metric_serializer<T>, class TAggregator = add_aggregator<T> >
class per_lcore_metric : public metric_base
{
public:
    using value_type = T;

    using storage_adapter = TStorageAdapter;

    using serializer = TSerializer;

    using aggregator = TAggregator;

    explicit per_lcore_metric(std::string name, metric_unit unit = metric_unit::NONE) :
        metric_base(std::move(name), unit) {

        for(auto& v : per_lcore_data) {
            v.set((T)0);
        }

        non_lcore_data.set((T)0);
    }

    ~per_lcore_metric() override = default;

    using metric_base::get_name;


    template < class TV >
    __inline void set(TV&& new_value) {
        auto lid = rte_lcore_id();

        if( lid == LCORE_ID_ANY ) {
            non_lcore_data.set(std::forward<TV>(new_value));
        } else {
            per_lcore_data[lid].set(std::forward<TV>(new_value));
        }
    }

    __inline void add(const T& v) {
        auto lid = rte_lcore_id();

        if( lid == LCORE_ID_ANY ) {
            non_lcore_data.add(v);
        } else {
            per_lcore_data[lid].add(v);
        }
    }

    __inline void inc() {
        auto lid = rte_lcore_id();

        if( lid == LCORE_ID_ANY ) {
            non_lcore_data.inc();
        } else {
            per_lcore_data[lid].inc();
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
    void serialize(json& v, const std::string& prefix) override {

        json value_obj;

        serializer::convert(value_obj, aggregator::convert(per_lcore_data, non_lcore_data));

        v.push_back({{"label", prefix}, {"value", std::move(value_obj)}, {"unit", metric_base::get_unit_str(get_unit())}});
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

     void serialize(json& v, const std::string& prefix) override;

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