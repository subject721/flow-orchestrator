

function init(endpoints)

    log(INFO, "Init called. Creating flows")

    for ep_index = 1,#endpoints
    do
        local endpoint = endpoints[ep_index]

        logf(INFO, "Creating flow for endpoint %s", endpoint:name())

        local packet_validator = flow.proc("ingress_packet_validator")
        local flow_classifier = flow.proc("flow_classifier")

        flow_classifier:set_param("test", "value")

        packet_validator:next(flow_classifier)

        endpoint:add_rx_proc(packet_validator)
    end


    log(INFO, "Init done")
end
