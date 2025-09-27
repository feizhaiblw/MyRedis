#include "../include/response.h"
#include <cstring>

void out_nil(Buffer& out){
    buf_append_u8(&out, TAG_NIL);
}

void out_str(Buffer& out, const char* data, size_t len){
    buf_append_u8(&out, TAG_STR);
    buf_append_u32(&out, len);
    buf_append(&out, (uint8_t*)data, len);
}

void out_int(Buffer& out, int64_t data){
    buf_append_u8(&out, TAG_INT);
    buf_append_i64(&out, data);
}

void out_dbl(Buffer& out, double data){
    buf_append_u8(&out, TAG_DBL);
    buf_append_dbl(&out, data);
}

void out_err(Buffer& out, uint32_t code, const std::string &data){
    buf_append_u8(&out, TAG_ERR);
    buf_append_u32(&out, code);
    buf_append_u32(&out, data.size());
    buf_append(&out, (const uint8_t*) data.data(), data.size());
}

void out_arr(Buffer& out,uint32_t len){
    buf_append_u8(&out, TAG_ARR);
    buf_append_u32(&out, len);
}

void response_begin(Buffer& out,size_t* header){
    *header=out.data_end-out.data_begin;
    buf_append_u32(&out,0);
}

size_t response_size(Buffer& out,size_t header){
    return out.data_end-out.data_begin-header-4;
}

void response_end(Buffer& out,size_t header){
    size_t msg_size=response_size(out,header);
    if(msg_size>MAX_MASSEGE){
        out.data_end=out.data_begin+header+4;
        out_err(out,message_too_long,"response is too big");
        msg_size = response_size(out, header);
    }
    uint32_t len = (uint32_t)msg_size;  // 转换为网络字节序
    memcpy(out.data_begin + header, &len, sizeof(len));  // 写入长度字段
}