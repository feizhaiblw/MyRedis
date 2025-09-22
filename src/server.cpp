#include <iostream>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <arpa/inet.h>
#include <cassert>
#include <unordered_map>
#include "../include/hashtable.h"
#include "../include/buffer.h"
#include "../include/common.h"
#include "../include/zset.h"

enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

enum{
    ERROR=1,
    message_too_long=2,
};

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

//record all connection's states
struct Conn{
    int fd=-1;
    bool want_write=false;
    bool want_read=false;
    bool want_close=false;
    Buffer coming_data;
    Buffer outgoing_data;
};

//value type
enum{
    T_INIT=0,
    T_SIR=1,  //string
    T_ZSET=2, //zset
};

static struct{
    HMap db;
}g_data;

struct Entry{
    HNode node;
    std::string key;
    uint32_t type=0;

    //waste space but easier
    std::string str;
    ZSet zset;
};


static Entry* entry_new(uint32_t type){
    Entry* entry=new Entry();
    entry->type=type;
    return entry;
}

static void entry_delete(Entry* entry){
    if(entry->type==T_ZSET){
        zset_clear(&entry->zset);
    }
    delete entry;
}

static bool entry_eq(HNode* a,HNode* b){
    struct Entry* le=container_of(a,struct Entry,node);
    struct Entry* re=container_of(b,struct Entry,node);
    return le->key==re->key;
}

static void out_nil(Buffer& out){
    buf_append_u8(&out, TAG_NIL);
}

static void out_str(Buffer& out, const char* data, size_t len){ 
    buf_append_u8(&out, TAG_STR);
    buf_append_u32(&out, len);
    buf_append(&out, (uint8_t*)data, len);
}

static void out_int(Buffer& out, int64_t data){
    buf_append_u8(&out, TAG_INT);
    buf_append_i64(&out, data);
}

static void out_dbl(Buffer& out, double data){
    buf_append_u8(&out, TAG_DBL);
    buf_append_dbl(&out, data);
}

static void out_err(Buffer& out, uint32_t code, const std::string &data){
    buf_append_u8(&out, TAG_ERR);
    buf_append_u32(&out, code);
    buf_append_u32(&out, data.size());
    buf_append(&out, (const uint8_t*) data.data(), data.size());
}

static void out_arr(Buffer& out,uint32_t len){
    buf_append_u8(&out, TAG_ARR);
    buf_append_u32(&out, len);
}

static bool cb_keys(HNode* node,void* arg){
    Buffer& out = *(Buffer*)arg;
    const std::string &key = container_of(node,Entry,node)->key;
    out_str(out, key.data(),key.size());
    return true;
}
static void do_keys(std::vector<std::string>& ,Buffer& out){
    out_arr(out,hm_size(&g_data.db));
    hm_foreach(&g_data.db,&cb_keys,(void*)&out);
}
static void do_get(std::vector<std::string>& cmd,Buffer& out){
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode=str_hash((uint8_t*) key.key.data(),key.key.size());
    HNode* node=hm_lookup(&g_data.db,&key.node,&entry_eq);
    if(!node){
        out_nil(out);
        return;
    }
    const std::string& val=container_of(node,struct Entry,node)->str;
    buf_append_u32(&out,val.size());
    out_str(out,val.data(),val.size());
}

static void do_set(std::vector<std::string>& cmd,Buffer& out){ 
    Entry entry;
    entry.key.swap(cmd[1]);
    entry.node.hcode=str_hash((uint8_t*) entry.key.data(),entry.key.size());
    HNode* node=hm_lookup(&g_data.db,&entry.node,entry_eq);
    if(node){
        container_of(node,struct Entry,node)->str.swap(cmd[2]);
    }else{
        Entry* entry=new Entry();
        entry->key.swap(cmd[1]);
        entry->str.swap(cmd[2]);
        entry->node.hcode=str_hash((uint8_t*) entry->key.data(),entry->key.size());
        hm_insert(&g_data.db,&entry->node);
    }
    return out_nil(out);
}

static void do_del(std::vector<std::string>& cmd, Buffer& out){
    Entry entry;
    entry.key.swap(cmd[1]);
    entry.node.hcode=str_hash((uint8_t*) entry.key.data(),entry.key.size());
    HNode* node=hm_delete(&g_data.db,&entry.node,entry_eq);
    if(node){
        delete(container_of(node,Entry,node));
    }
    return out_int(out,node?1:0);
}

static void do_request(std::vector<std::string>& cmd, Buffer& out) {
    if(cmd.empty()) return;
    if(cmd[0]=="set"&&cmd.size()==3){
        return do_set(cmd,out);
    }else if(cmd[0]=="get"&&cmd.size()==2){
        return do_get(cmd,out);
    }else if(cmd[0]=="del"&&cmd.size()==2){
        return do_del(cmd,out);
    }else{
        return out_err(out,ERROR,"ERROR in do_request");
    }
}

static void response_begin(Buffer& out,size_t* header){
    *header=out.data_end-out.data_begin;
    buf_append_u32(&out,0);
}

static size_t response_size(Buffer& out,size_t header){
    return out.data_end-out.data_begin-header-4;
}

static void response_end(Buffer& out,size_t header){
    size_t msg_size=response_size(out,header);
    if(msg_size>MAX_MASSEGE){
        out.data_end=out.data_begin+header+4;
        out_err(out,message_too_long,"response is too big");
        msg_size = response_size(out, header);
    }
    uint32_t len = (uint32_t)msg_size;  // 转换为网络字节序
    memcpy(out.data_begin + header, &len, sizeof(len));  // 写入长度字段
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
        conn->want_close = true;
        return false;
    }
    size_t header_pos=0;
    response_begin(conn->outgoing_data,&header_pos);
    do_request(cmd, conn->outgoing_data);
    response_end(conn->outgoing_data,header_pos);
    buf_consume(&conn->coming_data, 4 + len);
    return true;
}

static int32_t read_all(int fd, char* buf, size_t count) {
    while (count > 0) {
        ssize_t n = read(fd, buf, count);
        if (n==-1) {
            return -1;
        }
        buf += n;
        count -= n;
    }
    return 0;
}

static int32_t write_all(int fd, const char* buf, size_t count){
    while (count > 0) {
        ssize_t n = write(fd, buf, count);
        if (n==-1) {
            return -1;
        }
        buf += n;
        count -= n;
    }
    return 0;
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
    assert(conn->outgoing_data.data_end > conn->outgoing_data.data_begin);
    size_t size=conn->outgoing_data.data_end - conn->outgoing_data.data_begin;
    ssize_t n = write(conn->fd, conn->outgoing_data.data_begin, size);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            conn->want_close = true;
        }
        return;
    }
    buf_consume(&conn->outgoing_data, (size_t)n);
    if (conn->outgoing_data.data_end == conn->outgoing_data.data_begin) {
        conn->want_write = false;
        conn->want_read = true;
    } else {
        conn->want_write = true;
    }
}
static void handle_read(Conn* conn) { 
    uint8_t buf[MAX_MASSEGE];
    ssize_t n = read(conn->fd, buf, MAX_MASSEGE);
    if (n <= 0) {
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            conn->want_close = true;
        }
        return;
    }
    buf_append(&conn->coming_data, buf, n);
    while(try_one_request(conn)){};//pipelined,handle all requests in one connection at once
    if (conn->outgoing_data.data_end > conn->outgoing_data.data_begin ) {
        conn->want_write = true;
        conn->want_read = false;
        return handle_write(conn);
    } else {
        conn->want_read = true;
    }
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
    conn->want_read=true;
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
    std::cout << "Listening on port 8080" << std::endl;
    std::vector<Conn*> fd2conn;
    std::vector<pollfd> pollf_args;
    while(true){
        pollf_args.clear();
        struct pollfd pollf={fd, POLLIN, 0};
        pollf_args.push_back(pollf);
        //find updated fd and put them in pollf_args
        for(Conn* conn: fd2conn){
            if(!conn){
                continue;
            }
            pollf={conn->fd, POLLERR, 0};
            if(conn->want_read){
                pollf.events |= POLLIN;
            }
            if(conn->want_write){
                pollf.events |= POLLOUT;
            }
            pollf_args.push_back(pollf);
        }
        //the only blocked call,return when err or event happen
        int n = poll(pollf_args.data(), pollf_args.size(), -1);
        if(n==-1&&errno==EINTR){
            continue;
        }
        if (n==-1) {
            die("Error polling");
        }
        if(pollf_args[0].revents & POLLIN){
            if(Conn *conn=handle_accept(fd)){
                if(fd2conn.size()<=conn->fd){
                    fd2conn.resize(conn->fd+1);
                }
                fd2conn[conn->fd]=conn;
            }
        }
        for(size_t i=1;i<pollf_args.size();++i){
            uint32_t ready=pollf_args[i].revents;
            Conn *conn=fd2conn[pollf_args[i].fd];
            if(ready & POLLIN){ 
                handle_read(conn);
            }
            if(ready & POLLOUT){
                handle_write(conn);  
            }
            if(ready & POLLERR||conn->want_close){
                std::cout<<"Closing connection"<<std::endl;
                close(conn->fd);
                fd2conn[conn->fd]=nullptr;
                delete conn;
            }
        }
    }
    close(fd);
    return 0;
}