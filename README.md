# flow-orchestrator

## About

This is currently just a platform for me to learn more about DPDK and to have a foundation for some experiments.
It's not really useful yet, but I will add more and more features in my free time™.

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


## Running

Make sure everything that's needed for running DPDK apps is configured properly. This includes having some supported network interfaces bound via vfio-pci or uio_igb, hugepages being available...

I have a dual port 10G network card from Intel (X710) which is supported via the i40e driver and it's currently the only hardware I've tested.

My NIC physical functions are `0000:03:00.0`, `0000:03:00.1` and the resulting command is:

```shell
./build/flow-orchestrator --devices 'eth&0000:03:00.0' 'eth&0000:03:00.1' --init-script ./test/test_flow01.lua --telemetry-interval
50 --telemetry-endpoint 'tcp://127.0.0.1:8123' -- -l 1,2,3,4
```

Now go and connect with a [flow-orchestrator-telemetry-client](https://github.com/subject721/flow-orchestrator-telemetry-client) instance and see some numbers rolling.