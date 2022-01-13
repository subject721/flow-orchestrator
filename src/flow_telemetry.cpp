#include <flow_telemetry.hpp>

#include <zmq.hpp>

metric_base::~metric_base() {

}


void metric_base::updated() {

}

void metric_group::add_metric(metric_base& m) {
    std::lock_guard<std::mutex> guard(lk);

    child_metrics.push_back(std::ref(m));

    m.owning_distributor = owning_distributor;
}

void metric_group::remove_metric(metric_base& m) {
    std::lock_guard<std::mutex> guard(lk);

    auto it = child_metrics.begin();

    while(it != child_metrics.end()) {
        if(std::addressof(it->get()) == std::addressof(m)) {
            it = child_metrics.erase(it);

            m.owning_distributor = nullptr;
        } else {
            ++it;
        }
    }
}

void metric_group::serialize(json& v) {
    std::lock_guard<std::mutex> guard(lk);

    for(auto& m : child_metrics) {
        json m_obj;

        m.get().serialize(m_obj);

        v[m.get().get_name()] = std::move(m_obj);
    }
}

struct telemetry_distributor::private_data
{
    zmq::context_t ctx;

    std::optional<zmq::socket_t> socket;

    std::mutex lk;

    std::list<std::reference_wrapper<metric_base>> metrics;

    std::chrono::high_resolution_clock::time_point start_time;
};

telemetry_distributor::telemetry_distributor(const std::string& endpoint_addr) {
    pdata = std::make_unique<private_data>();

    pdata->socket.emplace(pdata->ctx, zmq::socket_type::pub);

    pdata->socket->bind(endpoint_addr);

    pdata->start_time = std::chrono::high_resolution_clock::now();
}

telemetry_distributor::~telemetry_distributor() {
    {
        std::lock_guard<std::mutex> guard(pdata->lk);

        pdata->metrics.clear();
    }

    pdata->socket->close();
    pdata->ctx.close();
}

void telemetry_distributor::add_metric(metric_base& m) {
    std::lock_guard<std::mutex> guard(pdata->lk);

    pdata->metrics.push_back(std::ref(m));

    m.owning_distributor = this;
}

void telemetry_distributor::remove_metric(metric_base& m) {
    std::lock_guard<std::mutex> guard(pdata->lk);

    auto it = pdata->metrics.begin();

    while(it != pdata->metrics.end()) {
        if(std::addressof(it->get()) == std::addressof(m)) {
            it = pdata->metrics.erase(it);

            m.owning_distributor = nullptr;
        } else {
            ++it;
        }
    }
}

void telemetry_distributor::do_update() {
    std::lock_guard<std::mutex> guard(pdata->lk);

    json msg_obj = json::object();

    const auto now = std::chrono::high_resolution_clock::now();

    const auto dur = (now - pdata->start_time);

    msg_obj["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(dur).count();

    for(auto& m : pdata->metrics) {
        json m_obj;

        m.get().serialize(m_obj);

        msg_obj[m.get().get_name()] = std::move(m_obj);
    }

    pdata->socket->send(zmq::buffer("metrics"), zmq::send_flags::sndmore);
    pdata->socket->send(zmq::buffer(msg_obj.dump()), zmq::send_flags::none);
}