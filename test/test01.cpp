#include <common/common.hpp>

#include <common/lua_common.hpp>

#include <iostream>

void func1(int arg1, const std::string& arg2) {
    log(LOG_INFO, "func1 -> arg1 = {}, arg2 = {}", arg1, arg2);
}


template < class TProxyType, class Callable, Callable c >
static void call_wrapped(TProxyType proxy_ptr, const std::vector<std::string>& args) {
    try {
        c(proxy_ptr, args);
    } catch(const std::exception& e) {

    } catch(...) {

    }
}

template < class T, class V = void >
struct arg_converter {};

template < class T >
struct arg_converter<T, std::enable_if_t<std::is_integral_v<T>>>
{
    static T convert(const std::string& s) {
        return (T) strtol(s.c_str(), nullptr, 10);
    }
};

template < class T >
struct arg_converter<T, std::enable_if_t<std::is_convertible_v<std::string, T>>>
{
    template < class V >
    static decltype(auto) convert(V&& s) {
        return std::forward<V>(s);
    }
};


template <class TFrom, class T>
struct arg_unwrapper;

template < class TFrom, class... TArgs >
struct arg_unwrapper<TFrom, void(TArgs...)>
{
    using proxy_type = void (*)(TArgs...);

    using arg_tuple = std::tuple<TArgs...>;


    template <size_t I, class... TFArgs>
    static void _call(proxy_type proxy_ptr, const std::vector<std::string>& arg_vec, TFArgs&&... args) {
        if constexpr (I < sizeof...(TArgs)) {
            _call<I + 1>(proxy_ptr, arg_vec, std::forward<TFArgs>(args)..., arg_converter<std::tuple_element_t<I, arg_tuple>>::convert(arg_vec[I]));
        } else {
            proxy_ptr(std::forward<TFArgs>(args)...);
        }
    }

    static void call (proxy_type proxy_ptr, const std::vector<TFrom>& args) {
        _call<0>(proxy_ptr, args);
    }
};

void test(void* proxy_ptr, const std::vector<std::string>& v) {

}

int main(int argc, char** argv) {

    arg_unwrapper<std::string, void(int, const std::string&)>::call(func1, {"42", "Hallo"});

    auto c = call_wrapped<arg_unwrapper<std::string, void(int, const std::string&)>::proxy_type, decltype(arg_unwrapper<std::string, void(int, const std::string&)>::call), arg_unwrapper<std::string, void(int, const std::string&)>::call>;

    c(func1, {"1337", "Foobar"});


    lua_engine lua;

    lua.load_stdlibs();

    lua.execute("log(INFO, \"Test info msg \" .. tostring(5))");
    lua.execute("log(WARN, \"Test warning\")");
    lua.execute(R"(
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
        lua.set("increment", 2);

        lua.call< int >("f", count);

        auto                     start    = std::chrono::high_resolution_clock::now();
        auto                     ret      = lua.call< int >("f", count);
        auto                     end      = std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds duration = end - start;

        log(LOG_INFO, "lua function returned {} and call took {}ns", ret.value_or(0), duration.count());
    } catch ( const std::exception& e ) {
        log(LOG_ERROR, "lua call richtig verkackt: {}", e.what());
    }

    return 0;
}
