#pragma once

namespace sched {

struct Task {
    int id = 0;
    double arrival_time = 0.0;
    double duration = 0.0;
    double est_duration = 0.0;
    int priority = 0;
    int cpu_demand = 1;
    double mem_demand = 0.0;
};

}
