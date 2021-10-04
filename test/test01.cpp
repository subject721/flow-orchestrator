#include <common/common.hpp>

#include <common/lua_common.hpp>

#include <iostream>


int main (int argc, char** argv) {


    lua_engine lua;

    lua.load_stdlibs ();

    lua.execute ("log(INFO, \"Test info msg \" .. tostring(5))");
    lua.execute ("log(WARN, \"Test warning\")");
    lua.execute (R"(
        function f(wurst)
            result = 0
            for i = 0, wurst-1 do
                result = (result + increment)

                if (result % 3 == 0) then
                    result = result / 2
                end
            end
            return result
        end

        )");


    size_t count = 10;

    try {
        lua.set ("increment", 2);

        lua.call< int > ("f", count);

        auto                     start    = std::chrono::high_resolution_clock::now ();
        auto                     ret      = lua.call< int > ("f", count);
        auto                     end      = std::chrono::high_resolution_clock::now ();

        std::chrono::nanoseconds duration = end - start;

        log (LOG_LEVEL_INFO,
             format ("lua function returned %1% and call took %2% ns") % ret.value_or (0) % duration.count ());
    } catch (const std::exception& e) { log (LOG_LEVEL_ERROR, format ("lua call richtig verkackt: %1%") % e.what ()); }

    return 0;
}
