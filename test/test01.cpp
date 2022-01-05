/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/common.hpp>
#include <common/generic_factory.hpp>
#include <common/file_utils.hpp>
#include <common/lua_common.hpp>

#include <iostream>
#include <numeric>


struct base_type
{
    base_type() = default;

    virtual ~base_type() = default;

    virtual void foo() {
        log(LOG_INFO, "base_type did foo");
    }
};

struct derived_type1 : public base_type
{
    explicit derived_type1(std::string name) : name(std::move(name)) {}

    ~derived_type1() override = default;

    void foo() override {
        log(LOG_INFO, "derived_type1({}) did foo", name);
    }

    std::string name;
};

struct derived_type2 : public base_type
{
    derived_type2() = default;

    explicit derived_type2(const std::string& name) : name(name) {}

    explicit derived_type2(const std::vector< std::string >& names) :
        name(std::accumulate(
            names.begin(), names.end(), std::string(), [](const std::string& s1, const std::string& s2) {
                return (!s1.empty() && !s2.empty()) ? fmt::format("{} {}", s1, s2) : (s1 + s2);
            })) {}

    ~derived_type2() override = default;

    void foo() override {
        log(LOG_INFO, "derived_type2({}) did foo", name);
    }

    std::string name;
};

int main(int argc, char** argv) {

    auto factory = create_factory< base_type >()
                       .append< base_type >("base_type")
                       .append< derived_type1 >("derived_type1")
                       .append< derived_type2 >("derived_type2");

    try {
        std::unique_ptr< base_type > d1p;
        std::shared_ptr< base_type > d2p1;
        derived_type2                d2p2;

        factory.construct_and_assign(d1p, "derived_type1", std::string("Norbert"));
        factory.construct_and_assign(d2p1, "derived_type2", std::string("Fischer"));
        factory.construct_and_assign(d2p2, "derived_type2", std::vector< std::string > {"Klaus", "Peter"});

        d1p->foo();
        d2p1->foo();
        d2p2.foo();


    } catch ( const std::exception& e ) { log(LOG_ERROR, "Factory test failed: {}", e.what()); }

    lua_engine lua;

    lua.load_stdlibs();

    lua.execute("log(INFO, \"Test info msg \" .. tostring(5))");
    lua.execute("log(WARN, \"Test warning\")");
    lua.execute(R"(
        num_executions = 0
        function f(wurst)
            result = 0
            for i = 0, wurst-1 do
                result = (result + increment)

                if (result % 3 == 0) then
                    result = result - 2
                end
            end

            num_executions = num_executions + 1

            log(INFO, "f : 1")
            bla()

            return result
        end

        function foobar()
            log(INFO, "foobar1")
        end

        )");

    lua.execute(R"(
        function foobar()
            log(INFO, "foobar2")
        end

        foobar()
    )");

    size_t count = 20;

    try {
        lua.set("increment", 3);

        lua.call< int >("f", count);

        auto start = std::chrono::high_resolution_clock::now();
        auto ret   = lua.call< int >("f", count);
        auto end   = std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds duration = end - start;

        log(LOG_INFO, "lua function returned {} and call took {}ns", ret.value_or(0), duration.count());
        log(LOG_INFO, "function was called {} times", lua.get<int>("num_executions").value_or(-1));
    } catch ( const std::exception& e ) { log(LOG_ERROR, "lua call richtig verkackt: {}", e.what()); }

    return 0;
}
