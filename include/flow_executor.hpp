#pragma once


#include "common/common.hpp"

#include "dpdk/dpdk_common.hpp"

#include "flow_base.hpp"

#include <forward_list>

template < class TExecutionPolicy, class TFlowManager >
class flow_executor
{
public:
    using execution_policy_type = TExecutionPolicy;

    using flow_manager_type = TFlowManager;

    using worker_callback_type = void (flow_manager_type::*)(const std::vector< size_t >&);

    flow_executor(flow_manager_type& flow_manager) : flow_manager(flow_manager) {}

    // TODO: Add support for multiple queues
    void setup(const std::vector< int >& endpoint_sockets,
               size_t                    num_distributors,
               std::vector< lcore_info > p_available_lcores) {
        available_lcores = std::move(p_available_lcores);

        size_t num_required_lcores = get_min_num_lcores(endpoint_sockets.size(), num_distributors, 1);

        if ( available_lcores.size() < num_required_lcores ) {
            throw std::runtime_error(fmt::format("insufficient number of cores: need {} but only {} are available",
                                                 num_required_lcores,
                                                 available_lcores.size()));
        }

        /*
         * Non-optimal lcore assignment scheme but should be sufficient for now. We can still blame the user for giving
         * us shitty lcores.
         */

        std::vector< lcore_info > remaining_lcores = available_lcores;

        for ( auto endpoint_socket : endpoint_sockets ) {
            auto best_match_it = std::find_if(
                remaining_lcores.begin(), remaining_lcores.end(), [endpoint_socket](const lcore_info& lc_info) {
                    return (endpoint_socket == lc_info.get_socket_id());
                });

            // No lcore on the same socket available. Take any
            if ( best_match_it == remaining_lcores.end() ) {
                log(LOG_WARN,
                    "No lcore on the same socket as the current endpoint available. This will degrade performance.");

                best_match_it = remaining_lcores.begin();
            }

            endpoint_lcores.push_back(*best_match_it);

            remaining_lcores.erase(best_match_it);
        }

        for ( size_t distributor_lcore_idx : seq(0, num_distributors) ) {
            distributor_lcores.push_back(remaining_lcores.at(distributor_lcore_idx));
        }

        for ( size_t index = 0; index < endpoint_lcores.size(); ++index ) {
            log(LOG_INFO, "Assigned lcore {} to endpoint {}", endpoint_lcores[index].get_lcore_id(), index);
        };

        for ( size_t index = 0; index < distributor_lcores.size(); ++index ) {
            log(LOG_INFO, "Assigned lcore {} to distributor {}", distributor_lcores[index].get_lcore_id(), index);
        }
    }

    void start(worker_callback_type endpoint_callback, worker_callback_type distributor_callback) {
        run_flag.store(true);

        // Find all execution targets per lcore

        std::vector< std::pair< uint32_t, std::vector< size_t > > > endpoints_per_lcore =
            get_unique_assignment(endpoint_lcores);

        std::vector< std::pair< uint32_t, std::vector< size_t > > > distributors_per_lcore =
            get_unique_assignment(distributor_lcores);


        for ( auto& distributor_mapping : distributors_per_lcore ) {
            std::vector< size_t > distributor_indicies = std::move(distributor_mapping.second);

            lcore_threads.emplace_front(distributor_mapping.first,
                                        [this, dist_ids = std::move(distributor_indicies), distributor_callback]() {
                                            while ( run_flag.load() ) {
                                                (flow_manager.*distributor_callback)(dist_ids);
                                            }
                                        });
        }

        for ( auto& endpoint_mapping : endpoints_per_lcore ) {
            std::vector< size_t > endpoint_indicies = std::move(endpoint_mapping.second);

            lcore_threads.emplace_front(endpoint_mapping.first,
                                        [this, dist_ids = std::move(endpoint_indicies), endpoint_callback]() {
                                            while ( run_flag.load() ) {
                                                (flow_manager.*endpoint_callback)(dist_ids);
                                            }
                                        });
        }
    }

    void stop() {
        run_flag.store(false);

        for ( auto& l : lcore_threads ) {
            l.join();
        }

        lcore_threads.clear();
    }

private:
    static constexpr size_t get_min_num_lcores(size_t num_flows, size_t num_distributors, size_t num_queues) {
        return ((execution_policy_type::num_workers_per_flow() * num_flows) +
                (execution_policy_type::num_workers_per_distributor() * num_distributors)) *
               num_queues;
    }

    static std::vector< std::pair< uint32_t, std::vector< size_t > > > get_unique_assignment(
        const std::vector< lcore_info >& component_lcore_mapping) {
        std::vector< std::pair< uint32_t, std::vector< size_t > > > component_indicies_per_lcore;

        for ( size_t endpoint_index = 0; endpoint_index < component_lcore_mapping.size(); ++endpoint_index ) {
            const auto& lc_info = component_lcore_mapping[endpoint_index];

            auto assignment_it = std::find_if(
                component_indicies_per_lcore.begin(),
                component_indicies_per_lcore.end(),
                [&lc_info](const auto& assignment_pair) { return (assignment_pair.first == lc_info.get_lcore_id()); });

            if ( assignment_it == component_indicies_per_lcore.end() ) {
                assignment_it = component_indicies_per_lcore.insert(component_indicies_per_lcore.end(),
                                                                    {lc_info.get_lcore_id(), {}});
            }

            assignment_it->second.push_back(endpoint_index);
        }

        return component_indicies_per_lcore;
    }

    static std::vector< uint32_t > find_components_for_lcore(uint32_t                         lcore_index,
                                                             const std::vector< lcore_info >& component_lcores) {
        std::vector< uint32_t > endpoint_indicies;

        for ( size_t endpoint_index = 0; endpoint_index < component_lcores.size(); ++endpoint_index ) {
            if ( component_lcores[endpoint_index].get_lcore_id() == lcore_index ) {
                endpoint_indicies.push_back(endpoint_index);
            }
        }

        return endpoint_indicies;
    }


    flow_manager_type& flow_manager;

    std::vector< lcore_info > available_lcores;

    std::vector< lcore_info > endpoint_lcores;

    std::vector< lcore_info > distributor_lcores;

    std::forward_list< lcore_thread > lcore_threads;

    std::atomic_bool run_flag;
};

struct reduced_core_policy
{

    static constexpr size_t num_workers_per_flow() {
        return 1;
    }

    static constexpr size_t num_workers_per_distributor() {
        return 1;
    }

    static constexpr size_t max_num_queues() {
        return 1;
    }
};
