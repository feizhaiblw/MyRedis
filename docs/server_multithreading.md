# Server 多线程使用说明

`src/server.cpp` 将事件循环与线程池结合，实现“主线程负责 I/O，工作线程负责命令执行”的模型。以下是关键流程的拆解。

## 1. 初始化阶段

```cpp
static ThreadPool* g_thread_pool = nullptr;
static int notify_fds[2] = {-1, -1};
...
if (pipe(notify_fds) == -1) die("pipe");
fd_set_nb(notify_fds[0]);
fd_set_nb(notify_fds[1]);

g_thread_pool = thread_pool_create(4);
```

- `pipe()` 创建一对管道用于工作线程 -> 主线程的轻量通知。
- `thread_pool_create(4)` 启动 4 个工作线程，数量可按需要调整。

## 2. 连接对象的状态

`struct Conn` 现包含：
- 原子标志 `want_read / want_write / want_close` 控制 poll 监听项
- `pending_tasks` 统计尚在执行的任务数，如非 0 则在关闭时等待
- `pthread_mutex_t mutex` 保护 `outgoing_data`
- 等等

## 3. 解析请求并提交任务

在 `handle_read()` 中调用 `try_one_request()`：

```cpp
std::vector<std::string> cmd;
if (parse_req(..., cmd) < 0) { want_close = true; return; }

auto* task = new RequestTask{conn, std::move(cmd)};
conn->pending_tasks.fetch_add(1);
thread_pool_submit(g_thread_pool, execute_request, task);
```

- 每条完整命令包装为 `RequestTask`
- `pending_tasks` 计数器确保连接关闭时等待所有后台任务完成

## 4. 工作线程执行逻辑

`execute_request(void* arg)`：

1. 加锁 `g_db_mutex` + `conn->mutex`，确保数据库和连接缓冲区的并发安全
2. 调用 `do_request(cmd, conn->outgoing_data)` 写入响应
3. 设置 `want_write = true, want_read = false`
4. `notify_main_thread()`：向管道写单字节，唤醒主循环
5. `pending_tasks` 自减，如减到 0 且连接标记关闭，再次通知主线程清理

## 5. 主循环中的通知处理

`pollf_args` 包含：监听 socket、`notify_fds[0]`、以及各连接的读/写事件。

```cpp
if (pollf_args[1].revents & POLLIN) {
    uint8_t buf[64];
    while (read(notify_fds[0], buf, sizeof buf) > 0) {}
}
```

- 读取管道数据即可将 poll 从阻塞中唤醒，随后遍历连接，根据 `want_write` 调用 `handle_write()` 输出响应。

## 6. 连接关闭处理

`close_conn()` 会：
- 置 `want_close = true`，并取消继续读写
- 若仍有任务在运行，则延迟释放，等待 `pending_tasks` 归零
- 否则直接调用 `finalize_conn()` 关闭 fd、删除对象

## 7. 线程池销毁

`thread_pool_destroy(g_thread_pool)` 在服务器退出前调用，确保所有工作线程安全 join，并释放资源。管道的读写端也在最后关闭。

## 8. 如何扩展

- **线程数配置**：可将 `thread_pool_create(4)` 改为读取配置或 `std::thread::hardware_concurrency()`。
- **任务上下文**：若需要更多处理上下文，可在 `RequestTask` 中追加字段。
- **错误处理**：`thread_pool_submit` 失败时当前逻辑会将连接标记为关闭，可按需求定制。

通过上述机制，主线程保持响应敏捷（专注于 accept/poll/write），CPU 密集或耗时逻辑由线程池处理，从而提升整体吞吐与响应能力。
