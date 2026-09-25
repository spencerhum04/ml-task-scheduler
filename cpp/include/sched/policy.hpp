#pragma once

#include <string>
#include <vector>

#include "sched/task.hpp"
#include "sched/worker.hpp"

namespace sched {

enum class Ordering { FIFO, Priority, SJF };
enum class Assignment { FirstFit, LeastLoaded };

class Policy {
public:
    explicit Policy(Ordering ordering = Ordering::FIFO,
                    Assignment assignment = Assignment::FirstFit)
        : ordering_(ordering), assignment_(assignment) {}
    virtual ~Policy() = default;

    virtual int select_task(const std::vector<Task>& queue, const std::vector<Worker>& workers,
                            double now) const;

    virtual int select_worker(const Task& task, const std::vector<Worker>& workers,
                              double now) const;

    virtual std::string name() const;

    Ordering ordering() const { return ordering_; }
    Assignment assignment() const { return assignment_; }

private:
    Ordering ordering_;
    Assignment assignment_;
};

}
