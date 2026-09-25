#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "sched/event_queue.hpp"
#include "sched/policy.hpp"
#include "sched/task.hpp"
#include "sched/worker.hpp"

namespace sched {

struct TaskRecord {
    int task_id = 0;
    int worker_id = -1;
    double arrival_time = 0.0;
    double start_time = 0.0;
    double finish_time = 0.0;
};

struct Metrics {
    std::size_t num_tasks = 0;
    double avg_jct = 0.0;
    double p95_jct = 0.0;
    double avg_wait = 0.0;
    double makespan = 0.0;
    double cpu_utilization = 0.0;
    double throughput = 0.0;
    std::uint64_t num_events = 0;
    std::vector<TaskRecord> records;
};

class Simulator {
public:
    Simulator(std::vector<Worker> workers, std::shared_ptr<Policy> policy);

    Metrics run(const std::vector<Task>& tasks);

    const std::vector<Worker>& workers() const { return workers_; }
    const Policy& policy() const { return *policy_; }

private:
    void reset(const std::vector<Task>& tasks);
    void handle(const Event& ev);
    void dispatch(double now);
    Metrics compute_metrics() const;

    std::vector<Worker> initial_workers_;
    std::shared_ptr<Policy> policy_;

    std::vector<Worker> workers_;
    std::vector<Task> tasks_;
    std::vector<Task> queue_;
    std::vector<int> queue_index_;
    std::vector<TaskRecord> records_;
    EventQueue events_;
    std::uint64_t next_seq_ = 0;
    std::uint64_t num_events_ = 0;
};

}
