/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021,  Stefan Seitz
 *
 */

#include <flow_processor.hpp>

#include <common/generic_factory.hpp>
#include <common/file_utils.hpp>

#include <rte_ip_frag.h>


flow_processor::flow_processor(std::string name, std::shared_ptr< dpdk_packet_mempool > mempool) :
    flow_node_base(std::move(name), std::move(mempool)) {}


ingress_packet_validator::ingress_packet_validator(std::string                            name,
                                                   std::shared_ptr< dpdk_packet_mempool > mempool,
                                                   std::shared_ptr< flow_database >       flow_database_ptr) :
    flow_processor(std::move(name), std::move(mempool)) {}

uint16_t ingress_packet_validator::process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) {
    for ( uint16_t packet_index = 0; packet_index < mbuf_vec.size(); ++packet_index ) {
        rte_mbuf* current_packet = mbuf_vec.begin()[packet_index];

        if ( likely(current_packet != nullptr) ) {
            bool drop_packet = false;

            uint16_t packet_len = rte_pktmbuf_pkt_len(current_packet);

            auto* packet_info = get_private_packet_info(current_packet);

            packet_info->new_flow = false;

            packet_info->src_endpoint_id = ctx.get_related_endpoint_id();
            packet_info->dst_endpoint_id = PORT_ID_BROADCAST;


            if ( likely(packet_len >= sizeof(rte_ether_hdr)) ) {

                rte_ether_hdr* ether_header = (rte_pktmbuf_mtod(current_packet, rte_ether_hdr*));

                uint16_t l2_len;
                uint16_t tci;
                uint16_t l2_proto;

                get_ether_header_info(ether_header, &l2_len, &tci, &l2_proto);

                if ( tci ) {
                    packet_info->vlan = rte_le_to_cpu_16(tci) & 0x0fffU;

                    rte_vlan_strip(current_packet);
                }

                l2_len = sizeof(rte_ether_hdr);

                packet_info->ether_type = l2_proto;
                packet_info->l3_offset  = l2_len;

                if ( l2_proto == ether_type_info< RTE_ETHER_TYPE_IPV4 >::ether_type_be ) {
                    drop_packet |= handle_ipv4_packet(rte_pktmbuf_mtod_offset(current_packet, const uint8_t*, l2_len),
                                                      packet_len - l2_len,
                                                      packet_info);
                }
            } else {
                drop_packet = true;
            }

            if ( unlikely(drop_packet) ) {
                rte_pktmbuf_free(current_packet);

                mbuf_vec.clear_packet(packet_index);
            }
        }
    }

    mbuf_vec.repack();

    return mbuf_vec.size();
}

void ingress_packet_validator::init(const flow_proc_builder& builder) {}

bool ingress_packet_validator::handle_ipv4_packet(const uint8_t*       ipv4_header_base,
                                                  uint16_t             l3_len,
                                                  packet_private_info* packet_info) {
    if ( unlikely(l3_len < sizeof(rte_ipv4_hdr)) )
        return true;

    const auto* ipv4_header = reinterpret_cast< const rte_ipv4_hdr* >(ipv4_header_base);

    uint16_t ipv4_header_len = rte_ipv4_hdr_len(ipv4_header);

    uint16_t ipv4_packet_len = rte_be_to_cpu_16(ipv4_header->total_length);

    packet_info->ipv4_type = ipv4_header->next_proto_id;

    // TODO: Handle reassembly of packets (maybe?)

    packet_info->is_fragment = rte_ipv4_frag_pkt_is_fragmented(ipv4_header);

    if ( !packet_info->is_fragment ) {
        if ( unlikely(l3_len < ipv4_packet_len) )
            return true;
    }

    packet_info->l4_offset = packet_info->l3_offset + ipv4_header_len;

    packet_info->ipv4_len = ipv4_packet_len;

    return false;
}

flow_classifier::flow_classifier(std::string                            name,
                                 std::shared_ptr< dpdk_packet_mempool > mempool,
                                 std::shared_ptr< flow_database >       flow_database_ptr) :
    flow_processor(std::move(name), std::move(mempool)), flow_database_ptr(std::move(flow_database_ptr)) {}

uint16_t flow_classifier::process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) {
    flow_database* fdb = flow_database_ptr.get();

    for ( uint16_t packet_index = 0; packet_index < mbuf_vec.size(); ++packet_index ) {
        rte_mbuf* current_packet = mbuf_vec.begin()[packet_index];

        flow_hash fhash = 0;

        if ( calc_flow_hash(current_packet, &fhash) ) {
            auto* packet_info = get_private_packet_info(current_packet);

            bool entry_created = false;

            packet_info->flow_info = fdb->get_or_create(fhash, entry_created);

            if ( likely(packet_info->flow_info) ) {
                if ( entry_created ) {
                    packet_info->flow_info->mark = 0;

                    const rte_ether_hdr* ether_header =
                        rte_pktmbuf_mtod_offset(current_packet, struct rte_ether_hdr*, 0);
                    const rte_ipv4_hdr* ipv4_header =
                        rte_pktmbuf_mtod_offset(current_packet, struct rte_ipv4_hdr*, packet_info->l3_offset);

                    rte_ether_addr_copy(&ether_header->d_addr, &packet_info->flow_info->ether_dst);
                    rte_ether_addr_copy(&ether_header->s_addr, &packet_info->flow_info->ether_src);

                    packet_info->flow_info->dst_addr = ipv4_header->dst_addr;
                    packet_info->flow_info->src_addr = ipv4_header->src_addr;

                    packet_info->new_flow = true;
                }

                // log(LOG_DEBUG, "pkt [flow {}] : {} -> {}", fhash, packet_info->flow_info->src_addr,
                // packet_info->flow_info->dst_addr);
            }
        }
    }

    return mbuf_vec.size();
}

void flow_classifier::init(const flow_proc_builder& builder) {}


struct lua_packet_accessor
{
    rte_mbuf* mbuf;

    packet_private_info* packet_info;

    flow_info_ipv4* flow_info;


    void init(rte_mbuf* mb) {
        mbuf = mb;

        packet_info = get_private_packet_info(mbuf);

        flow_info = packet_info->flow_info;
    }

    bool is_arp() const noexcept {
        return (packet_info->ether_type == ether_type_info< RTE_ETHER_TYPE_ARP >::ether_type_be);
    }

    bool is_ipv4() const noexcept {
        return (packet_info->ether_type == ether_type_info< RTE_ETHER_TYPE_IPV4 >::ether_type_be);
    }

    bool is_udp() const noexcept {
        return is_ipv4() && (packet_info->ipv4_type == IP_PROTO_UDP);
    }

    bool is_tcp() const noexcept {
        return is_ipv4() && (packet_info->ipv4_type == IP_PROTO_TCP);
    }

    bool is_icmp() const noexcept {
        return is_ipv4() && (packet_info->ipv4_type == IP_PROTO_ICMP);
    }

    uint32_t get_dst_ipv4() const noexcept {
        return (flow_info != nullptr) ? flow_info->dst_addr : 0;
    }

    uint32_t get_src_ipv4() const noexcept {
        return (flow_info != nullptr) ? flow_info->src_addr : 0;
    }
};

lua_packet_filter::lua_packet_filter(std::string                            name,
                                     std::shared_ptr< dpdk_packet_mempool > mempool,
                                     std::shared_ptr< flow_database >       flow_database_ptr) :
    flow_processor(std::move(name), std::move(mempool)), flow_database_ptr(std::move(flow_database_ptr)) {}

uint16_t lua_packet_filter::process(mbuf_vec_base& mbuf_vec, flow_proc_context& ctx) {


    for(auto packet : mbuf_vec) {
        lua_packet_accessor packet_accessor;

        packet_accessor.init(packet);

        auto result = process_function.call(packet_accessor);

        if(result.status() != sol::call_status::ok) {
            sol::error err = result;

            log(LOG_INFO, "lua process call failed {}", err.what());
        }
    }

    return mbuf_vec.size();
}

void lua_packet_filter::init(const flow_proc_builder& builder) {

    auto lua_script_filename = builder.get_param("script_filename");

    if ( !lua_script_filename.has_value() ) {
        throw std::runtime_error("script_filename not set");
    }

    lua.load_stdlibs();

    auto script_content = load_file_as_string(lua_script_filename.value());

    lua.execute(script_content, lua_script_filename.value());

    auto init_func = lua.get< sol::function >("init");

    if ( init_func ) {
        init_func->call(get_name());
    } else {
        log(LOG_WARN, "lua packet filter script {} has no init function", lua_script_filename.value());
    }

    auto proc_func = lua.get< sol::function >("process");

    if ( !proc_func.has_value() ) {
        throw std::runtime_error(fmt::format("{} does not expose a process function", lua_script_filename.value()));
    } else {
        process_function = proc_func.value();
    }

    lua.set_function("ipv4_to_str", [](uint32_t ipv4) -> std::string {
            return ipv4_to_str(ipv4);
       });

    lua.get().new_usertype< lua_packet_accessor >("packet",
                                                  //sol::no_constructor,
                                                  "is_arp",
                                                  &lua_packet_accessor::is_arp,
                                                  "is_ipv4",
                                                  &lua_packet_accessor::is_ipv4,
                                                  "is_udp",
                                                  &lua_packet_accessor::is_udp,
                                                  "is_tcp",
                                                  &lua_packet_accessor::is_tcp,
                                                  "is_icmp",
                                                  &lua_packet_accessor::is_icmp,
                                                  "get_dst_ipv4",
                                                  &lua_packet_accessor::get_dst_ipv4,
                                                  "get_src_ipv4",
                                                  &lua_packet_accessor::get_src_ipv4);

}


static auto packet_proc_factory = create_factory< flow_processor >()
                                      .append< ingress_packet_validator >("ingress_packet_validator")
                                      .append< flow_classifier >("flow_classifier")
                                      .append< lua_packet_filter >("lua_packet_filter");

std::unique_ptr< flow_processor > create_flow_processor(std::shared_ptr< flow_proc_builder >          proc_builder,
                                                        const std::shared_ptr< dpdk_packet_mempool >& mempool,
                                                        const std::shared_ptr< flow_database >&       flow_database) {
    std::unique_ptr< flow_processor > instance;

    packet_proc_factory.construct_and_assign(
        instance, proc_builder->get_class_name(), proc_builder->get_instance_name(), mempool, flow_database);

    instance->init(*proc_builder);

    return instance;
}