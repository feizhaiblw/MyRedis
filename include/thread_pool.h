#pragma once

#include <cstddef>
#include <pthread.h>

using ThreadTaskFn = void(*)(void*);

struct ThreadPool;

ThreadPool* thread_pool_create(size_t worker_count);
void thread_pool_destroy(ThreadPool* pool);
void thread_pool_submit(ThreadPool* pool, ThreadTaskFn fn, void* arg);
