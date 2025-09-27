#include "../include/thread_pool.h"

#include <queue>
#include <vector>
#include <stdexcept>

struct ThreadTask {
    ThreadTaskFn fn;
    void* arg;
};

struct ThreadPool {
    std::vector<pthread_t> workers;
    std::queue<ThreadTask> tasks;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool stop;
};

static void* thread_pool_worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (true) {
        pthread_mutex_lock(&pool->mutex);
        while (!pool->stop && pool->tasks.empty()) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        if (pool->stop && pool->tasks.empty()) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        ThreadTask task = pool->tasks.front();
        pool->tasks.pop();
        pthread_mutex_unlock(&pool->mutex);
        task.fn(task.arg);
    }
    return nullptr;
}

ThreadPool* thread_pool_create(size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument("worker_count must be > 0");
    }
    ThreadPool* pool = new ThreadPool();
    pool->stop = false;
    pthread_mutex_init(&pool->mutex, nullptr);
    pthread_cond_init(&pool->cond, nullptr);
    pool->workers.resize(worker_count);
    for (size_t i = 0; i < worker_count; ++i) {
        if (pthread_create(&pool->workers[i], nullptr, thread_pool_worker, pool) != 0) {
            pool->stop = true;
            pthread_cond_broadcast(&pool->cond);
            for (size_t j = 0; j < i; ++j) {
                pthread_join(pool->workers[j], nullptr);
            }
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond);
            delete pool;
            throw std::runtime_error("failed to create thread");
        }
    }
    return pool;
}

void thread_pool_destroy(ThreadPool* pool) {
    if (!pool) {
        return;
    }
    pthread_mutex_lock(&pool->mutex);
    pool->stop = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
    for (pthread_t& worker : pool->workers) {
        pthread_join(worker, nullptr);
    }
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    delete pool;
}

void thread_pool_submit(ThreadPool* pool, ThreadTaskFn fn, void* arg) {
    if (!pool || !fn) {
        throw std::invalid_argument("invalid task submission");
    }
    pthread_mutex_lock(&pool->mutex);
    pool->tasks.push({fn, arg});
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}
