#include "sched/simulator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace sched {

namespace {

double percentile(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const double pos = q * static_cast<double>(v.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = std::min(lo + 1, v.size() - 1);
    return v[lo] + (pos - static_cast<double>(lo)) * (v[hi] - v[lo]);
}

}

Simulator::Simulator(std::vector<Worker> workers, std::shared_ptr<Policy> policy)
    : initial_workers_(std::move(workers)), policy_(std::move(policy)) {
    if (!policy_) throw std::invalid_argument("policy must not be null");
    if (initial_workers_.empty()) throw std::invalid_argument("need at least one worker");
    for (auto& w : initial_workers_) {
        if (w.speed <= 0.0) throw std::invalid_argument("worker speed must be > 0");
        if (w.cores <= 0) throw std::invalid_argument("worker cores must be > 0");
        if (w.mem < 0.0) throw std::invalid_argument("worker mem must be >= 0");
        w.free_cores = w.cores;
        w.free_mem = w.mem;
        w.num_running = 0;
        w.finish_time_sum = 0.0;
    }
    workers_ = initial_workers_;
}

void Simulator::reset(const std::vector<Task>& tasks) {
    for (const auto& t : tasks) {
        const std::string which = "task " + std::to_string(t.id);
        if (!(t.duration > 0.0)) throw std::invalid_argument(which + ": duration must be > 0");
        if (t.arrival_time < 0.0) throw std::invalid_argument(which + ": arrival_time must be >= 0");
        if (t.cpu_demand <= 0) throw std::invalid_argument(which + ": cpu_demand must be > 0");
        if (t.mem_demand < 0.0) throw std::invalid_argument(which + ": mem_demand must be >= 0");
        const bool placeable = std::any_of(initial_workers_.begin(), initial_workers_.end(),
                                           [&](const Worker& w) { return w.can_ever_fit(t); });
        if (!placeable) throw std::invalid_argument(which + ": does not fit on any worker");
    }

    workers_ = initial_workers_;
    tasks_ = tasks;
    queue_.clear();
    queue_index_.clear();
    records_.assign(tasks_.size(), TaskRecord{});
    events_ = EventQueue{};
    next_seq_ = 0;
    num_events_ = 0;

    for (int i = 0; i < static_cast<int>(tasks_.size()); ++i) {
        records_[i].task_id = tasks_[i].id;
        records_[i].arrival_time = tasks_[i].arrival_time;
        events_.push({tasks_[i].arrival_time, EventType::Arrival, next_seq_++, i, -1});
    }
}

Metrics Simulator::run(const std::vector<Task>& tasks) {
    reset(tasks);
    while (!events_.empty()) {
        const double now = events_.top().time;
        while (!events_.empty() && events_.top().time == now) {
            const Event ev = events_.top();
            events_.pop();
            handle(ev);
        }
        dispatch(now);
    }
    if (!queue_.empty()) {
        throw std::runtime_error(policy_->name() + " left " + std::to_string(queue_.size()) +
                                 " task(s) queued with nothing left to run");
    }
    return compute_metrics();
}

void Simulator::handle(const Event& ev) {
    ++num_events_;
    const Task& t = tasks_[ev.task_index];
    if (ev.type == EventType::Arrival) {
        queue_.push_back(t);
        queue_index_.push_back(ev.task_index);
        return;
    }
    Worker& w = workers_[ev.worker_index];
    w.free_cores += t.cpu_demand;
    w.free_mem += t.mem_demand;
    w.num_running -= 1;
    w.finish_time_sum -= ev.time;
}

void Simulator::dispatch(double now) {
    while (!queue_.empty()) {
        const int qi = policy_->select_task(queue_, workers_, now);
        if (qi < 0) break;
        if (qi >= static_cast<int>(queue_.size())) {
            throw std::out_of_range("select_task returned " + std::to_string(qi) +
                                    " for a queue of size " + std::to_string(queue_.size()));
        }
        const int wi = policy_->select_worker(queue_[qi], workers_, now);
        if (wi < 0) break;
        if (wi >= static_cast<int>(workers_.size())) {
            throw std::out_of_range("select_worker returned " + std::to_string(wi) + " for " +
                                    std::to_string(workers_.size()) + " workers");
        }
        Worker& w = workers_[wi];
        const Task& t = queue_[qi];
        if (!w.fits(t)) {
            throw std::runtime_error("select_worker put task " + std::to_string(t.id) +
                                     " on worker " + std::to_string(w.id) +
                                     ", which lacks free capacity");
        }

        const int ti = queue_index_[qi];
        const double finish = now + t.duration / w.speed;
        w.free_cores -= t.cpu_demand;
        w.free_mem -= t.mem_demand;
        w.num_running += 1;
        w.finish_time_sum += finish;
        records_[ti].worker_id = w.id;
        records_[ti].start_time = now;
        records_[ti].finish_time = finish;
        events_.push({finish, EventType::Completion, next_seq_++, ti, wi});

        queue_.erase(queue_.begin() + qi);
        queue_index_.erase(queue_index_.begin() + qi);
    }
}

Metrics Simulator::compute_metrics() const {
    Metrics m;
    m.num_tasks = tasks_.size();
    m.num_events = num_events_;
    m.records = records_;
    if (tasks_.empty()) return m;

    std::vector<double> jcts;
    jcts.reserve(tasks_.size());
    double wait_sum = 0.0;
    double busy_core_seconds = 0.0;
    double first_arrival = records_[0].arrival_time;
    double last_finish = records_[0].finish_time;
    for (std::size_t i = 0; i < records_.size(); ++i) {
        const auto& r = records_[i];
        jcts.push_back(r.finish_time - r.arrival_time);
        wait_sum += r.start_time - r.arrival_time;
        busy_core_seconds += tasks_[i].cpu_demand * (r.finish_time - r.start_time);
        first_arrival = std::min(first_arrival, r.arrival_time);
        last_finish = std::max(last_finish, r.finish_time);
    }

    double jct_sum = 0.0;
    for (double j : jcts) jct_sum += j;
    int total_cores = 0;
    for (const auto& w : initial_workers_) total_cores += w.cores;

    const double n = static_cast<double>(tasks_.size());
    m.avg_jct = jct_sum / n;
    m.p95_jct = percentile(jcts, 0.95);
    m.avg_wait = wait_sum / n;
    m.makespan = last_finish - first_arrival;
    if (m.makespan > 0.0) {
        m.cpu_utilization = busy_core_seconds / (total_cores * m.makespan);
        m.throughput = n / m.makespan;
    }
    return m;
}

}
