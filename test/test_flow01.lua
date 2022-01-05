
StreamProc = {}
function StreamProc:new(name, terminates_flow)
    o = {}
    o.next_proc = nil
    o.name = name
    if terminates_flow ~= nil then
        o.terminates_flow = terminates_flow
    else
        o.terminates_flow = false
    end
    setmetatable(o, self)
    self.__index = self
    log(INFO, "StreamProc constructor")
    return o
end

function StreamProc:next(p)
    log(INFO, "StreamProc next() of proc " .. self.name)
    self.next_proc = p

    if p.terminates_flow then
        log(INFO, "Flow terminated")

        return
    end

    return self.next_proc
end

function new_flow_classifier()
    log(INFO, "Creating flow classifier")
    return StreamProc:new("flow classifier")
end

function distribute()
    return StreamProc:new("distributor", true)
end

function get_endpoints()
    return {
        {
            index = 0,
            rx_flow = function()
                return StreamProc:new("endpoint")
            end
        }
    }
end

function init(endpoints)

    log(INFO, "Init called. Creating flows")


    for ep_index = 1,#endpoints
    do
        local endpoint = endpoints[ep_index]
        logf(INFO, "Creating flow for endpoint %s", endpoint:name())

        local p = flow.proc("flow_classifier")

        logf(INFO, "Creating flow for endpoint %s", tostring(p))
    end

--     local endpoints = get_endpoints()
--
--     for i,endpoint in ipairs(endpoints) do
--         log(INFO, "Creating flow for endpoint " .. endpoint.index)
--
--         endpoint.rx_flow():next(new_flow_classifier()):next(distribute())
--     end

    log(INFO, "Init done")
end
