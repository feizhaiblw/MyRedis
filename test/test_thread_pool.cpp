#include "include/thread_pool.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <unordered_set>

#include <pthread.h>

struct SharedState {
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> in_flight{0};
    std::atomic<int> peak{0};
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    std::vector<pthread_t> threads;
};

struct TaskArg {
    SharedState* state;
    std::chrono::milliseconds work_time;
};

static void counting_task(void* arg) {
    TaskArg* task = static_cast<TaskArg*>(arg);
    SharedState* state = task->state;

    state->started.fetch_add(1, std::memory_order_relaxed);

    int current = state->in_flight.fetch_add(1, std::memory_order_relaxed) + 1;
    int prev_peak = state->peak.load(std::memory_order_relaxed);
    while (current > prev_peak && !state->peak.compare_exchange_weak(prev_peak, current, std::memory_order_relaxed)) {}

    pthread_mutex_lock(&state->lock);
    state->threads.push_back(pthread_self());
    pthread_mutex_unlock(&state->lock);

    std::this_thread::sleep_for(task->work_time);

    state->in_flight.fetch_sub(1, std::memory_order_relaxed);
    state->finished.fetch_add(1, std::memory_order_relaxed);
}

static void basic_thread_pool_test() {
    constexpr int worker_count = 4;
    constexpr int task_count = 64;

    ThreadPool* pool = thread_pool_create(worker_count);
    SharedState state;
    std::vector<TaskArg> tasks(task_count);

    for (int i = 0; i < task_count; ++i) {
        tasks[i].state = &state;
        tasks[i].work_time = std::chrono::milliseconds(5 + (i % 3));
        thread_pool_submit(pool, counting_task, &tasks[i]);
    }

    while (state.finished.load(std::memory_order_relaxed) < task_count) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    thread_pool_destroy(pool);

    if (state.started.load() != task_count || state.finished.load() != task_count) {
        std::cerr << "Not all tasks completed" << std::endl;
        std::exit(1);
    }

    if (state.peak.load() <= 1) {
        std::cerr << "Expected concurrent execution, peak concurrency=" << state.peak.load() << std::endl;
        std::exit(1);
    }

    pthread_mutex_lock(&state.lock);
    std::unordered_set<pthread_t> unique_threads(state.threads.begin(), state.threads.end());
    pthread_mutex_unlock(&state.lock);

    if (unique_threads.size() < static_cast<size_t>(worker_count)) {
        std::cerr << "Not all worker threads were utilized" << std::endl;
        std::exit(1);
    }
}

static void destruction_waits_for_tasks() {
    ThreadPool* pool = thread_pool_create(2);
    SharedState state;
    TaskArg arg{&state, std::chrono::milliseconds(50)};

    thread_pool_submit(pool, counting_task, &arg);
    thread_pool_submit(pool, counting_task, &arg);

    // Destroy should block until both tasks finish
    thread_pool_destroy(pool);

    if (state.finished.load() != 2) {
        std::cerr << "Thread pool destroyed before tasks finished" << std::endl;
        std::exit(1);
    }
}

int main() {
    basic_thread_pool_test();
    destruction_waits_for_tasks();
    std::cout << "Thread pool tests passed" << std::endl;
    return 0;
}
