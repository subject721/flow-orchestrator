

function init(endpoints)

    log(INFO, "Init called. Creating flows")

    for ep_index = 1,#endpoints
    do
        local endpoint = endpoints[ep_index]

        logf(INFO, "Creating flow for endpoint %s", endpoint:get_instance_name())

        local packet_validator = flow.proc("ingress_packet_validator", "validator")
        local flow_classifier = flow.proc("flow_classifier", "classifier")
        local lua_filter = flow.proc("lua_packet_filter", "filter01")

        lua_filter:set_param("script_filename", "test/filter01.lua")

        packet_validator:next(flow_classifier)
        flow_classifier:next(lua_filter)

        endpoint:add_rx_proc(packet_validator)
    end


    log(INFO, "Init done")
end
