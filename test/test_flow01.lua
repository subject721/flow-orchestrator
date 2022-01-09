

function init(endpoints)

    log(INFO, "Init called. Creating flows")

    for ep_index = 1,#endpoints
    do
        local endpoint = endpoints[ep_index]

        logf(INFO, "Creating flow for endpoint %s", endpoint:name())

        endpoint:add_rx_proc(flow.proc("ingress_packet_validator"))
    end

    log(INFO, "Init done")
end
