
#include <common/common.hpp>

#include <dpdk/dpdk_common.hpp>
#include <dpdk/dpdk_ethdev.hpp>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <config.h>

#include <iostream>

#include <algorithm>

#include <csignal>

#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>

namespace po = boost::program_options;

using boost::format;


static std::mutex global_lock;

static std::condition_variable global_cond;

static std::deque<int> signal_queue;

static void signal_handler(int signal) {
    {
        std::lock_guard<std::mutex> guard(global_lock);

        signal_queue.push_back(signal);

        global_cond.notify_all();
    }
}

template < class TRep, class TPeriod >
static bool wait_for_signal(int& signal, std::chrono::duration<TRep, TPeriod> timeout) {
    std::unique_lock<std::mutex> lk(global_lock);

    if(global_cond.wait_for(lk, timeout, [](){
        return !signal_queue.empty();
    })) {
        signal = signal_queue.front();

        signal_queue.pop_front();

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


    bool should_exit;

    std::vector< std::string > dpdk_options;

    std::vector<std::string> device_names;

    uint32_t pool_size;
    uint16_t cache_size;
    uint16_t dataroom_size;
    uint16_t private_size;

    std::shared_ptr<dpdk_mempool> mempool;


};


int main(int argc, char** argv) {

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGUSR1, signal_handler);

    try {
        flow_orchestrator_app app(argc, argv);

        return app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

        return 0;
    }
}

flow_orchestrator_app::flow_orchestrator_app(int argc, char** argv) : should_exit(false) {

    parse_args(argc, argv);

    if (!should_exit) {
        setup();
    }
}

int flow_orchestrator_app::run() {
    lcore_thread t1(1, []() { std::cout << "hello from lcore " << rte_lcore_id() << std::endl; });


    bool run_state = true;

    while(run_state) {
        int signal_num;

        if(wait_for_signal(signal_num, std::chrono::milliseconds(200))) {
            if(signal_num == SIGINT || signal_num == SIGTERM) {
                run_state = false;
            }
        }

        // Do some other important stuff;
    }


    t1.join();



    return 0;
}

void flow_orchestrator_app::parse_args(int argc, char** argv) {
    po::options_description desc("Allowed options");

    desc.add_options()
        ("help", "produce help message")
        ("dpdk-options", po::value< std::vector< std::string > >(), "dpdk options")
        ("devices", po::value<std::vector<std::string> >());

    po::positional_options_description p;

    p.add("dpdk-options", -1);

    po::variables_map vm;

    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

    po::notify(vm);

    if (vm.count("help")) {
        std::cout << "TODO: Print help!" << std::endl;

        should_exit = true;

        return;
    }

    // Always add program name first
    dpdk_options.push_back(std::string(argv[0]));

    if (vm.count("dpdk-options")) {
        auto positional_args = vm["dpdk-options"].as< std::vector< std::string > >();

        for (const std::string& arg : positional_args) {
            dpdk_options.push_back(arg);
        }
    }

    if(!vm.count("devices")) {
        throw std::runtime_error("at least one device required");
    }

    device_names = vm["devices"].as<std::vector<std::string> >();

    pool_size = (1 << 14);
    cache_size = 128;
    dataroom_size = 2000;
    private_size = 256;
}

void flow_orchestrator_app::setup() {
    dpdk_options.push_back("--no-shconf");
    dpdk_options.push_back("--in-memory");

    dpdk_eal_init(dpdk_options);

    mempool = std::make_shared<dpdk_mempool>(pool_size, cache_size, dataroom_size, private_size);

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
