#pragma once

#include <cstdint>
#include <queue>
#include <tuple>
#include <vector>

namespace sched {

enum class EventType { Completion = 0, Arrival = 1 };

struct Event {
    double time;
    EventType type;
    std::uint64_t seq;
    int task_index;
    int worker_index;
};

struct EventLater {
    bool operator()(const Event& a, const Event& b) const {
        return std::tie(a.time, a.type, a.seq) > std::tie(b.time, b.type, b.seq);
    }
};

using EventQueue = std::priority_queue<Event, std::vector<Event>, EventLater>;

}
