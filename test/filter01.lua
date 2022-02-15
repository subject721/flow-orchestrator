function init(processor_name)

    logf(INFO, "initializing lua packet processor %s", processor_name)

end

function process(packet)

    if packet:is_icmp() then
        logf(INFO, "pkt: src_ip = %s, dst_ip = %s, ", ipv4_to_str(packet:get_src_ipv4()), ipv4_to_str(packet:get_dst_ipv4()))
    end

    if packet:get_src_endpoint_id() == 0 then
        return 1
    else
        return 0
    end
end