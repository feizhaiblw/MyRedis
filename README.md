# MyRedis 项目说明

## 概述
MyRedis 是一个使用 C++ 编写的轻量级 Redis 风格服务器，基于 `poll` 的事件循环与自实现线程池，可在多线程环境下稳定处理并发的二进制协议请求。项目实现了字符串键值与有序集合两类数据模型，并提供完整的客户端、测试脚本与构建流程，便于在本地复现与扩展。

## 目录结构

```
MyRedis/
├── include/              # 头文件（数据结构、协议、命令处理等）
├── src/                  # 服务器、客户端与核心实现
├── test/                 # 单元测试与并发压测脚本
├── docs/                 # 设计文档（如 server_multithreading.md）
├── Makefile              # 构建与测试入口
├── README.md             # 项目说明（本文档）
└── server / client       # 编译生成的可执行文件
```

## 核心功能

- **网络服务**：非阻塞 TCP socket + `poll` 事件循环，结合线程池串联命令解析与执行，支持管道化请求。
- **数据模型**：
  - 字符串：`SET`、`GET`、`DEL`、`SETEX`、`EXPIRE`、`KEYS` 等。
  - 有序集合：`ZADD`、`ZREM`、`ZSCORE`、`ZRANK`、`ZCARD`、`ZRANGE`、`ZRANGEBYSCORE`。
- **过期机制**：`SETEX`/`EXPIRE` 支持秒级 TTL，结合最小堆与哈希表完成懒惰 + 定时清理。
- **线程池**：可配置工作线程数量（默认 4），用于异步执行命令并回写响应。
- **客户端**：`src/client.cpp` 提供交互式 CLI，负责握手、协议编码与响应解析。
- **测试**：
  - C++ 单元测试：`make test` 自动编译并运行 AVL、ZSet、TTL、线程池等模块验证。
  - 并发压力脚本：`test/test_multithread.py` 通过多线程同时执行 `SET/GET`，验证服务器在高并发下的正确性与稳定性。

## 构建与运行

### 编译

```bash
# 构建服务器（默认目标）
make

# 构建客户端
make client

# 清理解构建产物
make clean
```

生成物位于项目根目录：`server`、`client`。

### 启动服务器

```bash
./server [port]
# 例如：./server 34567 （默认端口 8080）
```

### 客户端交互

```bash
./client 127.0.0.1 34567
> set foo bar
(nil)
> get foo
"bar"
> zadd ranks 100 alice
(nil)
> zrange ranks 0 -1 WITHSCORES
["alice",
 100]
```

客户端支持引号与转义，便于发送包含空格的参数；所有命令均按项目自定义的二进制协议传输。

## 测试与验证

### 单元测试

```bash
make test
```

该命令会逐个编译并执行 AVL、ZSet、TTL、线程池等测试，用于验证数据结构和命令实现的正确性。

### 并发压力测试

```bash
python3 test/test_multithread.py --host 127.0.0.1 --port 34567 \
    --threads 8 --ops 50
```

脚本会启动多个线程，每个线程循环执行 `SET` 与 `GET`，所有线程均通过验证即表示服务端在高并发场景下能够稳定运行。
