#include <iostream>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <limits>
#include <algorithm>
#include <atomic>
#include <memory>
#include <pthread.h>
#include <exception>
#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <errno.h>
#include "../include/buffer.h"
#include "../include/common.h"
#include "../include/dlist.h"
#include "../include/db.h"
#include "../include/thread_pool.h"
#include "../include/commands.h"
#include "../include/response.h"

static void msg(const char* msg){
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char* msg){
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char* msg) {
    perror(msg);
    exit(1);
}

static void fd_set_nb(int fd){
    errno=0;
    int flag = fcntl(fd, F_GETFL,0);
    if(errno){
        die("fcntl wrong");
    }
    flag |= O_NONBLOCK;
    errno = 0;
    (void)fcntl(fd, F_SETFL, flag);
    if(errno){
        die("fcntl wrong");
    }
}

//指向进程唯一的线程池实例，初始化为 nullptr，服务器启动后会创建它，后续请求任务都提交给这个池子分发给工作线程执行。
static ThreadPool* g_thread_pool = nullptr;
//保存一对管道文件描述符（读端、写端，初始为 -1 表示未建立），工作线程处理完任务后通过写端写入单字节，唤醒 poll 循环监听的读端，实现“线程→主线程”通知
static int notify_fds[2] = {-1, -1};

struct Conn;

//包装一次客户端请求要处理的数据；包含关联的连接对象指针 conn，以及解析出的命令参数列表 cmd，作为线程池任务入口的参数。
struct RequestTask {
    Conn* conn;
    std::vector<std::string> cmd;
};

//用于写管道唤醒事件循环。
static void notify_main_thread();

//线程池工作函数的声明，线程池回调会调用它，参数是 RequestTask*，内部完成命令执行、写回响应并根据需要再次唤醒主线程。
static void execute_request(void* arg);

//record all connection's states
struct Conn{
    int fd=-1;
    std::atomic<bool> want_write{false};
    std::atomic<bool> want_read{true};
    std::atomic<bool> want_close{false};
    Buffer coming_data;
    Buffer outgoing_data;
    int64_t last_active_ms=0;
    ListNode idle_node;
    bool in_idle_list=false;
    pthread_mutex_t mutex;
    std::atomic<int> pending_tasks{0};

    Conn(){
        list_node_init(&idle_node);
        pthread_mutex_init(&mutex, nullptr);
    }

    ~Conn(){
        pthread_mutex_destroy(&mutex);
    }
};

static constexpr int64_t IDLE_TIMEOUT_MS = 60 * 1000;

class IdleList {
public:
    IdleList(){
        list_head_init(&head_);
    }

    void add(Conn* conn){
        if(!conn){
            return;
        }
        if(conn->in_idle_list){
            touch(conn);
            return;
        }
        list_push_back(&head_, &conn->idle_node);
        conn->in_idle_list = true;
    }

    void touch(Conn* conn){
        if(!conn){
            return;
        }
        if(!conn->in_idle_list){
            add(conn);
            return;
        }
        list_remove(&conn->idle_node);
        list_push_back(&head_, &conn->idle_node);
    }

    void remove(Conn* conn){
        if(!conn || !conn->in_idle_list){
            return;
        }
        list_remove(&conn->idle_node);
        conn->in_idle_list = false;
    }

    Conn* front(){
        ListNode* node = list_front(&head_);
        if(!node){
            return nullptr;
        }
        return container_of(node, Conn, idle_node);
    }

    bool empty() const {
        return list_is_empty(&head_);
    }

private:
    ListNode head_{};
};

static IdleList idle_list;

static void touch_conn(Conn* conn){
    if(!conn){
        return;
    }
    conn->last_active_ms = now_ms();
    idle_list.touch(conn);
}

static void finalize_conn(Conn* conn, std::vector<Conn*>& fd2conn){
    if(!conn){
        return;
    }
    idle_list.remove(conn);
    int fd = conn->fd;
    if(fd >= 0){
        close(fd);
    }
    if(fd >= 0 && fd < (int)fd2conn.size()){
        fd2conn[fd] = nullptr;
    }
    delete conn;
}

static void close_conn(Conn* conn, std::vector<Conn*>& fd2conn){
    if(!conn){
        return;
    }
    conn->want_close.store(true, std::memory_order_release);
    conn->want_read.store(false, std::memory_order_release);
    conn->want_write.store(false, std::memory_order_release);
    idle_list.remove(conn);
    if(conn->pending_tasks.load(std::memory_order_acquire) > 0){
        return;
    }
    finalize_conn(conn, fd2conn);
}

static void close_idle_connections(std::vector<Conn*>& fd2conn, int64_t now){
    if(idle_list.empty()){
        return;
    }
    Conn* head = idle_list.front();
    while(head && now - head->last_active_ms >= IDLE_TIMEOUT_MS){
        std::cout<<"Closing idle connection"<<std::endl;
        close_conn(head, fd2conn);
        head = idle_list.front();
        if(head && head->want_close.load(std::memory_order_acquire) && head->pending_tasks.load(std::memory_order_acquire) > 0){
            break;
        }
    }
}
static int compute_idle_timeout(int64_t now){
    Conn* head = idle_list.front();
    if(!head){
        return -1;
    }
    int64_t expire_at = head->last_active_ms + IDLE_TIMEOUT_MS;
    if(expire_at <= now){
        return 0;
    }
    int64_t diff = expire_at - now;
    if(diff > std::numeric_limits<int>::max()){
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(diff);
}

static int compute_poll_timeout(){
    int64_t now = now_ms();
    int idle_timeout = compute_idle_timeout(now);
    pthread_mutex_lock(&g_db_mutex);
    int expire_timeout = db_time_until_next_expire(now);
    pthread_mutex_unlock(&g_db_mutex);
    if(idle_timeout < 0){
        return expire_timeout;
    }
    if(expire_timeout < 0){
        return idle_timeout;
    }
    return std::min(idle_timeout, expire_timeout);
}

static int32_t parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out);
static bool try_one_request(Conn* conn) {
    if(conn->coming_data.data_end - conn->coming_data.data_begin < 4){
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, conn->coming_data.data_begin, 4);
    if(conn->coming_data.data_end - conn->coming_data.data_begin < 4 + len){
        return false;
    }

    if(len > MAX_MASSEGE){
        return false;
    }

    std::vector<std::string> cmd;
    if(parse_req(conn->coming_data.data_begin+4, len, cmd)<0){
        conn->want_close.store(true, std::memory_order_release);
        return false;
    }
    buf_consume(&conn->coming_data, 4 + len);
    auto* task = new RequestTask();
    task->conn = conn;
    task->cmd = std::move(cmd);
    conn->pending_tasks.fetch_add(1, std::memory_order_acq_rel);
    try {
        thread_pool_submit(g_thread_pool, execute_request, task);
    } catch (...) {
        conn->pending_tasks.fetch_sub(1, std::memory_order_acq_rel);
        delete task;
        conn->want_close.store(true, std::memory_order_release);
        return false;
    }
    return true;
}


//作用是让后台工作线程能唤醒主事件循环：
//
//还没建立通知管道（写端 notify_fds[1] 仍为 -1），直接返回。
//向管道写入一个字节 b = 1。写操作会触发主线程在 poll 中监听到该管道的可读事件，从而跳出阻塞循环，及时处理某个连接上已经准备好的响应。
//(void) n 只是显式丢弃 write 的返回值，避免“变量未使用”的编译警告。
static void notify_main_thread(){
    if(notify_fds[1] == -1){
        return;
    }
    uint8_t b = 1;
    ssize_t n = write(notify_fds[1], &b, sizeof(b));
    (void)n;
}

static void execute_request(void* arg){
    std::unique_ptr<RequestTask> task(static_cast<RequestTask*>(arg));
    Conn* conn = task->conn;
    pthread_mutex_lock(&g_db_mutex);
    pthread_mutex_lock(&conn->mutex);
    size_t header_pos = 0;
    response_begin(conn->outgoing_data, &header_pos);
    do_request(task->cmd, conn->outgoing_data);
    response_end(conn->outgoing_data, header_pos);
    conn->want_write.store(true, std::memory_order_release);
    conn->want_read.store(false, std::memory_order_release);
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&g_db_mutex);

    notify_main_thread();

    int prev = conn->pending_tasks.fetch_sub(1, std::memory_order_acq_rel);
    if(prev == 1 && conn->want_close.load(std::memory_order_acquire)){
        notify_main_thread();
    }
}


static bool read_u32(const uint8_t*& cur, const uint8_t* end, uint32_t& out){
    if(cur+4>end) return false;
    memcpy(&out, cur, 4);
    cur+=4;
    return true;
}

static bool read_str(const uint8_t*& cur, const uint8_t* end, uint32_t len, std::string& out){
    if(cur+len>end) return false;
    out.assign((const char*)cur, len);
    cur+=len;
    return true;
}

//nstr+len1+str1+len2+str2+...
static int32_t parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out){
    const uint8_t* cur=data;
    const uint8_t* end=cur+size;
    uint32_t nstr=0;
    if(!read_u32(cur,end,nstr)) return -1;
    if(nstr>MAX_MASSEGE) return -1;
    out.clear();
    while(out.size()<nstr){
        uint32_t str_len=0;
        if(!read_u32(cur,end,str_len)) return -1;
        out.emplace_back();
        if(!read_str(cur,end,str_len,out.back())) return -1;
    }
    if(cur!=end) return -1;
    return 0;
}

static void handle_write(Conn* conn) {
    pthread_mutex_lock(&conn->mutex);
    size_t size = conn->outgoing_data.data_end - conn->outgoing_data.data_begin;
    if(size == 0){
        conn->want_write.store(false, std::memory_order_release);
        pthread_mutex_unlock(&conn->mutex);
        return;
    }
    ssize_t n = write(conn->fd, conn->outgoing_data.data_begin, size);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            conn->want_close.store(true, std::memory_order_release);
        }
        pthread_mutex_unlock(&conn->mutex);
        return;
    }
    buf_consume(&conn->outgoing_data, (size_t)n);
    bool drained = (conn->outgoing_data.data_end == conn->outgoing_data.data_begin);
    if (drained) {
        conn->want_write.store(false, std::memory_order_release);
        conn->want_read.store(true, std::memory_order_release);
    } else {
        conn->want_write.store(true, std::memory_order_release);
    }
    pthread_mutex_unlock(&conn->mutex);
    if(n > 0){
        touch_conn(conn);
    }
}

static void handle_read(Conn* conn) {
    uint8_t buf[MAX_MASSEGE];
    ssize_t n = read(conn->fd, buf, MAX_MASSEGE);
    if (n <= 0) {
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            conn->want_close.store(true, std::memory_order_release);
        }
        return;
    }
    buf_append(&conn->coming_data, buf, n);
    touch_conn(conn);
    while(try_one_request(conn)){};//pipelines
    conn->want_read.store(true, std::memory_order_release);
}

static Conn* handle_accept(int fd) {
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    int client_fd=accept(fd, (struct sockaddr*)&addr, &len);
    if (client_fd==-1) {
        return nullptr;
    }
    std::cout<<"Accepted connection from "<<inet_ntoa(addr.sin_addr)<<":"<<ntohs(addr.sin_port)<<std::endl;
    fd_set_nb(client_fd);
    Conn* conn=new Conn();
    conn->fd=client_fd;
    conn->want_read.store(true, std::memory_order_release);
    conn->last_active_ms = now_ms();
    idle_list.add(conn);
    return conn;
}

int main() {
    int fd=socket(AF_INET, SOCK_STREAM, 0);
    if (fd==-1) {
        std::cout << "Error creating socket" << std::endl;
        return -1;
    }
    fd_set_nb(fd);
    int val=1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr))==-1) {
        std::cout << "Error binding socket" << std::endl;
        return -1;
    }
    if (listen(fd, SOMAXCONN)==-1) {
        die("Error listening");
    }
    if(pipe(notify_fds) == -1){
        die("pipe");
    }
    fd_set_nb(notify_fds[0]);
    fd_set_nb(notify_fds[1]);

    try {
        g_thread_pool = thread_pool_create(4);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to create thread pool: " << ex.what() << std::endl;
        close(fd);
        close(notify_fds[0]);
        close(notify_fds[1]);
        return -1;
    }

    std::cout << "Listening on port 8080" << std::endl;
    std::vector<Conn*> fd2conn;
    std::vector<pollfd> pollf_args;
    while(true){
        int64_t loop_now = now_ms();
        pthread_mutex_lock(&g_db_mutex);
        db_purge_expired(loop_now);
        pthread_mutex_unlock(&g_db_mutex);
        close_idle_connections(fd2conn, loop_now);
        pollf_args.clear();
        struct pollfd pollf={fd, POLLIN, 0};
        pollf_args.push_back(pollf);
        struct pollfd notify_poll = {notify_fds[0], POLLIN, 0};
        pollf_args.push_back(notify_poll);
        //find updated fd and put them in pollf_args
        for(Conn* conn: fd2conn){
            if(!conn){
                continue;
            }
            pollf={conn->fd, POLLERR, 0};
            if(conn->want_read.load(std::memory_order_acquire)){
                pollf.events |= POLLIN;
            }
            if(conn->want_write.load(std::memory_order_acquire)){
                pollf.events |= POLLOUT;
            }
            pollf_args.push_back(pollf);
        }
        int timeout_ms = compute_poll_timeout();
        int n = poll(pollf_args.data(), pollf_args.size(), timeout_ms);
        if(n==-1&&errno==EINTR){
            continue;
        }
        if (n==-1) {
            die("Error polling");
        }
        if(n==0){
            continue;
        }
        if(pollf_args[0].revents & POLLIN){
            if(Conn *conn=handle_accept(fd)){
                if(fd2conn.size() <= static_cast<size_t>(conn->fd)){
                    fd2conn.resize(static_cast<size_t>(conn->fd)+1);
                }
                fd2conn[conn->fd]=conn;
            }
        }
        if(pollf_args.size()>1 && (pollf_args[1].revents & POLLIN)){
            uint8_t buf[64];
            while(read(notify_fds[0], buf, sizeof(buf)) > 0){}
        }
        for(size_t i=2;i<pollf_args.size();++i){
            uint32_t ready=pollf_args[i].revents;
            Conn *conn=fd2conn[pollf_args[i].fd];
            if(!conn){
                continue;
            }
            if(ready & POLLIN){
                handle_read(conn);
            }
            if(ready & POLLOUT){
                handle_write(conn);  
            }
            if((ready & POLLERR)||conn->want_close.load(std::memory_order_acquire)){
                std::cout<<"Closing connection"<<std::endl;
                close_conn(conn, fd2conn);
            }
        }
    }
    close(fd);
    thread_pool_destroy(g_thread_pool);
    close(notify_fds[0]);
    close(notify_fds[1]);
    return 0;
}
