#include "sched/policy.hpp"

namespace sched {

int Policy::select_task(const std::vector<Task>& queue, const std::vector<Worker>&,
                        double) const {
    if (queue.empty()) return -1;
    if (ordering_ == Ordering::FIFO) return 0;
    int best = 0;
    for (int i = 1; i < static_cast<int>(queue.size()); ++i) {
        if (ordering_ == Ordering::Priority ? queue[i].priority > queue[best].priority
                                            : queue[i].est_duration < queue[best].est_duration) {
            best = i;
        }
    }
    return best;
}

int Policy::select_worker(const Task& task, const std::vector<Worker>& workers,
                          double) const {
    int best = -1;
    for (int i = 0; i < static_cast<int>(workers.size()); ++i) {
        if (!workers[i].fits(task)) continue;
        if (assignment_ == Assignment::FirstFit) return i;
        if (best < 0 || workers[i].load() < workers[best].load()) best = i;
    }
    return best;
}

std::string Policy::name() const {
    std::string s;
    switch (ordering_) {
        case Ordering::FIFO: s = "fifo"; break;
        case Ordering::Priority: s = "priority"; break;
        case Ordering::SJF: s = "sjf"; break;
    }
    switch (assignment_) {
        case Assignment::FirstFit: s += "+first_fit"; break;
        case Assignment::LeastLoaded: s += "+least_loaded"; break;
    }
    return s;
}

}
