

function init(endpoints)

    log(INFO, "Init called. Creating flows")

    for ep_index = 1,#endpoints
    do
        local endpoint = endpoints[ep_index]

        logf(INFO, "Creating flow for endpoint %s", endpoint:name())

        endpoint:next(flow.proc("flow_classifier")):next(flow.proc("router"))
    end

    log(INFO, "Init done")
end
