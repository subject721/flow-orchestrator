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

    std::unique_ptr< flow_endpoint_base > create_endpoint(const std::string& type,
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

    flow_manager flow_mgr;

    std::string init_script_name;
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

    int rc;

    try {
        rte_openlog_stream(log_proxy::get_cfile());

        flow_orchestrator_app app(argc, argv);

        rc = app.run();
    } catch ( const std::exception& e ) {
        log(LOG_ERROR, "Fatal error! Aborting... Error Message : \"{}\"", e.what());

        rc = -1;
    }

    rte_eal_cleanup();

    return rc;
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

    log(LOG_INFO, "Starting flows");

    flow_mgr.start();

    while ( run_state ) {
        int signal_num;

        if ( wait_for_signal(signal_num, std::chrono::milliseconds(200)) ) {
            if ( signal_num == SIGINT || signal_num == SIGTERM ) {
                run_state = false;
            }
        }



        // Do some other important stuff;
    }

    log(LOG_INFO, "Stopping flows");

    flow_mgr.stop();

    rte_eal_mp_wait_lcore();

    return 0;
}

static const std::string DPDK_OPTIONS_FLAG = "dpdk-options";
static const std::string DEVICES_FLAG = "devices";
static const std::string INIT_SCRIPT_FLAG = "init-script";

void flow_orchestrator_app::parse_args(int argc, char** argv) {

    using namespace std;
    using namespace boost;
    namespace po = boost::program_options;

    po::options_description desc("Command line options");

    desc.add_options()("help", "print help")(
        DPDK_OPTIONS_FLAG.c_str(), po::value< std::vector< std::string > >()->multitoken(), "dpdk options")(
        DEVICES_FLAG.c_str(), po::value< std::vector< std::string > >()->multitoken(), "Devices to use")(
        INIT_SCRIPT_FLAG.c_str(), po::value< std::string >(), "Init script to load");

    po::positional_options_description p;

    p.add(DPDK_OPTIONS_FLAG.c_str(), -1);

    po::variables_map vm;

    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

    po::notify(vm);

    if ( vm.count("help") ) {

        desc.print(std::cout);

        should_exit = true;

        return;
    }

    if(vm.count(INIT_SCRIPT_FLAG)) {
        init_script_name = vm[INIT_SCRIPT_FLAG].as<std::string>();
    }

    // Always add program name first
    dpdk_options.push_back(std::string(argv[0]));

    if ( vm.count(DPDK_OPTIONS_FLAG) ) {
        auto positional_args = vm[DPDK_OPTIONS_FLAG].as< std::vector< std::string > >();

        for ( const std::string& arg : positional_args ) {
            dpdk_options.push_back(arg);
        }
    }

    if ( !vm.count(DEVICES_FLAG) ) {
        throw std::runtime_error("at least one device required");
    }

    device_names = vm[DEVICES_FLAG].as< std::vector< std::string > >();

    pool_size     = (1 << 14);
    cache_size    = 128;
    dataroom_size = RTE_MBUF_DEFAULT_BUF_SIZE;
    private_size  = align_to_next_multiple(sizeof(packet_private_info), (size_t) RTE_MBUF_PRIV_ALIGN);

}


void flow_orchestrator_app::setup() {
    log(LOG_INFO, "Settings up devices and flows");

    //dpdk_options.push_back("--no-shconf");
    //dpdk_options.push_back("--in-memory");

    std::vector<dev_info> dev_info_list;

    for ( const auto& interface_identifier : device_names ) {
        dev_info info;

        for ( auto token_it = boost::make_split_iterator(interface_identifier, first_finder("&", boost::is_iequal()));
              token_it != boost::split_iterator< std::string::const_iterator >();
              ++token_it ) {

            if ( !info.dev_type_str.has_value() ) {
                info.dev_type_str = boost::copy_range< std::string >(*token_it);
            } else if ( !info.dev_id_str.has_value() ) {
                info.dev_id_str = boost::copy_range< std::string >(*token_it);
            } else if ( !info.dev_options_str.has_value() ) {
                info.dev_options_str = boost::copy_range< std::string >(*token_it);
            } else {
                throw std::invalid_argument(
                    fmt::format("device specification {} has invalid format", interface_identifier));
            }
        }

        if ( !info.dev_type_str.has_value() ) {
            throw std::invalid_argument(
                fmt::format("device specification {} has no device type specifier", interface_identifier));
        }
        if ( !info.dev_id_str.has_value() ) {
            throw std::invalid_argument(
                fmt::format("device specification {} has no device id specifier", interface_identifier));
        }

        dev_info_list.push_back(std::move(info));
    }

    for(const auto& info : dev_info_list) {
        if(info.dev_type_str.value() == "eth") {
            dpdk_options.push_back("-a");
            dpdk_options.push_back(info.dev_id_str.value());
        }
    }

    dpdk_eal_init(dpdk_options);

    log(LOG_INFO, "Creating packet memory pool: Capacity: {}, Cache Size: {}, Dataroom Size: {}, Private Size: {}",
        pool_size, cache_size, dataroom_size, private_size);


    mempool = std::make_shared< dpdk_mempool >(pool_size, cache_size, dataroom_size, private_size);

    // Create endpoint instances
    std::vector< std::unique_ptr< flow_endpoint_base > > endpoints;

    for(const auto& info : dev_info_list) {
        endpoints.emplace_back(create_endpoint(info.dev_type_str.value(), info.dev_id_str.value(), info.dev_options_str.value_or("")));
    }

    if(!init_script_name.empty()) {
        init_script_handler init_handler;

        log(LOG_INFO, "Loading flow init script {}", init_script_name);

        init_handler.load_init_script(init_script_name);

        auto flow_program = init_handler.build_program(std::move(endpoints));

        flow_mgr.load(std::move(flow_program));
    }

    log(LOG_INFO, "Setup done");
}

std::unique_ptr< flow_endpoint_base > flow_orchestrator_app::create_endpoint(const std::string& type,
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

        log(LOG_INFO, "Creating device instance {} of type {} as port {}", id, type, dev_port_id);

        auto eth_dev = std::make_unique< dpdk_ethdev >(dev_port_id, 0, 1024, 1024, 1, 1, mempool);

        // Endpoint nodes have their name set to the id of the actual interface (at least for now)
        return std::make_unique< eth_dpdk_endpoint >(id, mempool, std::move(eth_dev));
    } else {
        throw std::invalid_argument(fmt::format("Invalid device type: {}", type));
    }
}
