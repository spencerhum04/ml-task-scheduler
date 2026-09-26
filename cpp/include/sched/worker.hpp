#pragma once

#include <limits>

#include "sched/task.hpp"

namespace sched {

struct Worker {
    Worker() = default;
    Worker(int id, double speed, int cores,
           double mem = std::numeric_limits<double>::infinity())
        : id(id), speed(speed), cores(cores), mem(mem), free_cores(cores), free_mem(mem) {}

    int id = 0;
    double speed = 1.0;
    int cores = 1;
    double mem = std::numeric_limits<double>::infinity();

    int free_cores = 1;
    double free_mem = std::numeric_limits<double>::infinity();
    int num_running = 0;
    double finish_time_sum = 0.0;

    bool fits(const Task& t) const {
        return t.cpu_demand <= free_cores && t.mem_demand <= free_mem + 1e-9;
    }
    bool can_ever_fit(const Task& t) const {
        return t.cpu_demand <= cores && t.mem_demand <= mem + 1e-9;
    }
    double load() const { return 1.0 - static_cast<double>(free_cores) / cores; }
    double remaining_work(double now) const { return finish_time_sum - num_running * now; }
};

}
