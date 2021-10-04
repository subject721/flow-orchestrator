# flow-orchestrator

## About

This is currently just a platform for me to learn more about DPDK and to have a foundation for some experiments.


## Building

Building should work with g++/clang++ as long as the version fully supports C++17.

For building you basically need to have the following things installed:

- meson
- ninja
- dpdk
- boost
- luajit
- sol2

NOTE: The dependencies must be installed in a way that meson is able to find them. In most cases this is via pkg-config


## Whatever

Currently there are some hardcoded flags in the meson file that disable the use of avx/avx2 instructions. This is the outcome of pure laziness (one of my test servers does not support avx/avx2)


