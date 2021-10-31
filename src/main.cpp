/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <common/common.hpp>
#include <common/network_utils.hpp>

#include <dpdk/dpdk_common.hpp>
#include <dpdk/dpdk_ethdev.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <config.h>

#include <iostream>

#include <algorithm>

#include <csignal>

#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>

#include <flow_base.hpp>
#include <flow_config.hpp>
#include <flow_manager.hpp>


static std::mutex global_lock;

static std::condition_variable global_cond;

static std::optional< std::deque< int > > signal_queue;

static void signal_handler(int signal) {
    {
        std::lock_guard< std::mutex > guard(global_lock);

        if ( signal_queue.has_value() ) {
            signal_queue->push_back(signal);

            global_cond.notify_all();
        } else {
            ::exit(-1);
        }
    }
}

template < class TRep, class TPeriod >
static bool wait_for_signal(int& signal, std::chrono::duration< TRep, TPeriod > timeout) {
    if ( !signal_queue.has_value() ) {
        ::exit(-1);
    }

    std::unique_lock< std::mutex > lk(global_lock);

    if ( global_cond.wait_for(lk, timeout, []() { return !signal_queue->empty(); }) ) {
        signal = signal_queue->front();

        signal_queue->pop_front();

        return true;
    } else {
        return false;
    }
}

class flow_orchestrator_app : noncopyable
{
public:
    flow_orchestrator_app(int argc, char** argv);

    int run();

private:
    void parse_args(int argc, char** argv);

    void setup();

    std::shared_ptr< flow_endpoint_base > create_endpoint(const std::string& type,
                                                          const std::string& id,
                                                          const std::string& options);

    bool should_exit;

    std::vector< std::string > dpdk_options;

    std::vector< std::string > device_names;

    uint32_t pool_size;
    uint16_t cache_size;
    uint16_t dataroom_size;
    uint16_t private_size;

    std::shared_ptr< dpdk_mempool > mempool;

    flow_manager flow_manager;
};


int main(int argc, char** argv) {

    signal_queue = std::deque< int >();

    std::string welcome_msg = fmt::format("Running Flow Orchestrator {}", VERSION_STR);

    size_t padding = welcome_msg.size() + 4;

    // Taken from the fmt library documentation. It is just too awesome to be not used!
    fmt::print(
        "┌{0:─^{2}}┐\n"
        "│{1: ^{2}}│\n"
        "└{0:─^{2}}┘\n",
        "",
        welcome_msg,
        padding);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGUSR1, signal_handler);

    try {
        flow_orchestrator_app app(argc, argv);

        return app.run();
    } catch ( const std::exception& e ) {
        std::cerr << e.what() << std::endl;

        return 0;
    }
}

flow_orchestrator_app::flow_orchestrator_app(int argc, char** argv) :
    should_exit(false), pool_size(0), cache_size(0), dataroom_size(0), private_size(0) {
    parse_args(argc, argv);

    if ( !should_exit ) {
        setup();
    }
}

int flow_orchestrator_app::run() {

    if ( should_exit )
        return 0;

    bool run_state = true;

    flow_manager.start();

    while ( run_state ) {
        int signal_num;

        if ( wait_for_signal(signal_num, std::chrono::milliseconds(200)) ) {
            if ( signal_num == SIGINT || signal_num == SIGTERM ) {
                run_state = false;
            }
        }

        // Do some other important stuff;
    }

    flow_manager.stop();

    return 0;
}

void flow_orchestrator_app::parse_args(int argc, char** argv) {

    using namespace std;
    using namespace boost;
    namespace po = boost::program_options;

    po::options_description desc("Command line options");

    desc.add_options()("help", "print help")(
        "dpdk-options", po::value< std::vector< std::string > >()->multitoken(), "dpdk options")(
        "devices", po::value< std::vector< std::string > >()->multitoken(), "Devices to use")(
        "init-script", po::value< std::string >(), "Init script to load");

    po::positional_options_description p;

    p.add("dpdk-options", -1);

    po::variables_map vm;

    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

    po::notify(vm);

    if ( vm.count("help") ) {
        std::cout << "TODO: Print help!" << std::endl;

        should_exit = true;

        return;
    }

    // Always add program name first
    dpdk_options.push_back(std::string(argv[0]));

    if ( vm.count("dpdk-options") ) {
        auto positional_args = vm["dpdk-options"].as< std::vector< std::string > >();

        for ( const std::string& arg : positional_args ) {
            dpdk_options.push_back(arg);
        }
    }

    if ( !vm.count("devices") ) {
        throw std::runtime_error("at least one device required");
    }

    device_names = vm["devices"].as< std::vector< std::string > >();

    pool_size     = (1 << 14);
    cache_size    = 128;
    dataroom_size = RTE_MBUF_DEFAULT_BUF_SIZE;
    private_size  = align_to_next_multiple(sizeof(packet_private_info), (size_t) RTE_MBUF_PRIV_ALIGN);
}

void flow_orchestrator_app::setup() {
    dpdk_options.push_back("--no-shconf");
    dpdk_options.push_back("--in-memory");

    dpdk_eal_init(dpdk_options);

    mempool = std::make_shared< dpdk_mempool >(pool_size, cache_size, dataroom_size, private_size);

    // Create endpoint instances
    std::vector< std::shared_ptr< flow_endpoint_base > > endpoints;

    for ( const auto& interface_identifier : device_names ) {
        std::optional< std::string > dev_type_str;
        std::optional< std::string > dev_id_str;
        std::optional< std::string > dev_options_str;

        for ( auto token_it = boost::make_split_iterator(interface_identifier, first_finder("%", boost::is_iequal()));
              token_it != boost::split_iterator< std::string::const_iterator >();
              ++token_it ) {

            if ( !dev_type_str.has_value() ) {
                dev_type_str = boost::copy_range< std::string >(*token_it);
            } else if ( !dev_id_str.has_value() ) {
                dev_id_str = boost::copy_range< std::string >(*token_it);
            } else if ( !dev_options_str.has_value() ) {
                dev_options_str = boost::copy_range< std::string >(*token_it);
            } else {
                throw std::invalid_argument(
                    fmt::format("device specification {} has invalid format", interface_identifier));
            }
        }

        if ( !dev_type_str.has_value() ) {
            throw std::invalid_argument(
                fmt::format("device specification {} has no device type specifier", interface_identifier));
        }
        if ( !dev_id_str.has_value() ) {
            throw std::invalid_argument(
                fmt::format("device specification {} has no device id specifier", interface_identifier));
        }

        endpoints.push_back(create_endpoint(dev_type_str.value(), dev_id_str.value(), dev_options_str.value_or("")));
    }

    //    std::string dev_name = device_names.front();
    //
    //    uint64_t dev_port_id = 0;
    //
    //    const auto all_devices = get_available_ethdev_ids();
    //
    //    auto dev_it = std::find_if(all_devices.begin(), all_devices.end(), [&dev_name](uint64_t port_id){
    //        return (dpdk_ethdev::get_device_info(port_id).get_name() == dev_name);
    //    });
    //
    //    if(dev_it != all_devices.end()) {
    //        dev_port_id = *dev_it;
    //    } else {
    //        throw std::runtime_error((boost::format("device %1% not available") % dev_name).str());
    //    }
    //
    //    test_dev = std::make_shared<dpdk_ethdev>(dev_port_id, 0, 1024, 1024, 1, 1, mempool);
    //
    //    test_dev->start();

    std::cout << "setup done" << std::endl;
}

std::shared_ptr< flow_endpoint_base > flow_orchestrator_app::create_endpoint(const std::string& type,
                                                                             const std::string& id,
                                                                             const std::string& options) {

    if ( type == "eth" ) {
        uint64_t dev_port_id = 0;

        const auto all_devices = get_available_ethdev_ids();

        auto dev_it = std::find_if(all_devices.begin(), all_devices.end(), [&id](uint64_t port_id) {
            return (dpdk_ethdev::get_device_info(port_id).get_name() == id);
        });

        if ( dev_it != all_devices.end() ) {
            dev_port_id = *dev_it;
        } else {
            throw std::runtime_error(fmt::format("ethernet device {} not available", id));
        }

        auto eth_dev = std::make_unique< dpdk_ethdev >(dev_port_id, 0, 1024, 1024, 1, 1, mempool);

        // Endpoint nodes have their name set to the id of the actual interface (at least for now)
        return std::make_shared< eth_dpdk_endpoint >(id, mempool, std::move(eth_dev));
    } else {
        throw std::invalid_argument(fmt::format("Invalid device type: {}", type));
    }
}
