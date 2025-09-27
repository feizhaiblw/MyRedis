# Thread Pool 概览

线程池位于 `src/thread_pool.cpp`，对外接口由 `include/thread_pool.h` 暴露。它提供一个固定数量的工作线程，用于异步处理提交的任务。

## 核心结构

- **ThreadPool**：内部持有
  - `std::vector<pthread_t> workers`：工作线程句柄
  - `std::queue<ThreadTask> tasks`：待执行任务队列
  - `pthread_mutex_t mutex` / `pthread_cond_t cond`：保护任务队列并协调线程休眠与唤醒
  - `bool stop`：用于通知工作线程安全退出
- **ThreadTask**：封装任务函数指针 `ThreadTaskFn` 与参数 `void* arg`

## 生命周期

1. `thread_pool_create(size_t worker_count)`
   - 初始化锁与条件变量
   - 启动 `worker_count` 个线程进入任务循环
2. `thread_pool_submit(ThreadPool*, ThreadTaskFn, void*)`
   - 将任务压入队列
   - `pthread_cond_signal` 唤醒一个休眠线程
3. `thread_pool_destroy(ThreadPool*)`
   - 设置 `stop = true`
   - 广播唤醒所有线程，等待它们退出
   - 销毁同步原语

## 工作线程逻辑

```cpp
while (true) {
    lock();
    while (!stop && tasks.empty()) cond_wait();
    if (stop && tasks.empty()) break;
    task = queue.front(); queue.pop();
    unlock();
    task.fn(task.arg);
}
```

- 当队列为空时线程进入等待；任务到来或销毁时通过条件变量唤醒。
- `stop` 标志确保销毁阶段不会丢任务，同时让线程干净退出。

## 与服务器的协作

服务器在启动时创建线程池并持有全局指针 `g_thread_pool`：

```cpp
g_thread_pool = thread_pool_create(4);
```

每当在 `try_one_request` 中解析出一条完整命令，就构造 `RequestTask` 并调用 `thread_pool_submit`。工作线程执行完命令后会：

1. 加锁写入 `Conn` 的响应缓冲区
2. 设置 `want_write` 标记，表示缓冲区已有数据可写
3. 调用 `notify_main_thread()` 往管道写一个字节，唤醒主 poll 循环

主线程在 `poll()` 中监听管道读端，一旦被唤醒就遍历连接写出响应，从而实现“事件循环 + 线程池”协作模型。

## 注意事项

- 任务函数必须能处理 `void*` 参数，并自行管理生命周期；线程池不会拷贝或释放任务数据。
- 当前实现使用 `pthread` 原语，在 POSIX 环境可直接运行；其他平台需要相应适配。
- 全局的 `g_db_mutex` 用于保护数据库访问，线程任务在执行命令时需注意加锁顺序以避免死锁。

