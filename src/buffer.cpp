#include "../include/buffer.h"
#include <cstring>

void buf_consume(Buffer* buf, size_t n){
    // 计算当前可用数据大小
    size_t data_size = buf->data_end - buf->data_begin;
    
    // 只消费实际可用的数据
    if (n > data_size) {
        n = data_size; // 消费所有可用数据
    }
    
    // 执行消费操作
    buf->data_begin += n;
    
    // 如果缓冲区为空，重置指针到起始位置
    if (buf->data_begin == buf->data_end) {
        buf->data_begin = buf->buffer_begin;
        buf->data_end = buf->buffer_begin;
    }
}
//data may still remain.
//to be honest,i am not sure about the specific meaning of MAX_MASSAGE but i think it is enough and won't trigger resize.
void buf_append(Buffer* buf, const uint8_t* data, size_t len){
    if (buf->data_end + len > buf->buffer_end) {
        size_t data_size = buf->data_end - buf->data_begin;
        if (buf->buffer_begin + data_size + len <= buf->buffer_end) {
            memmove(buf->buffer_begin, buf->data_begin, data_size);
            buf->data_begin = buf->buffer_begin;
            buf->data_end = buf->buffer_begin + data_size;
        } else {
            size_t new_capacity = (buf->buffer_end - buf->buffer_begin) * 2 + len;
            uint8_t *new_buffer = new uint8_t[new_capacity];
            memcpy(new_buffer, buf->data_begin, data_size);
            delete[] buf->buffer_begin;
            buf->buffer_begin = new_buffer;
            buf->buffer_end = new_buffer + new_capacity;
            buf->data_begin = new_buffer;
            buf->data_end = new_buffer + data_size;
        }
    }
    memcpy(buf->data_end, data, len);
    buf->data_end += len;
}

void buf_append_u8(Buffer* buf, uint8_t data){
    buf_append(buf, &data, sizeof(data));
}

void buf_append_u32(Buffer* buf, uint32_t data){
    buf_append(buf, (const uint8_t *)&data, sizeof(uint32_t));
}

void buf_append_i64(Buffer* buf, uint64_t data){
    buf_append(buf, (const uint8_t *)&data, sizeof(uint64_t));
}

void buf_append_dbl(Buffer* buf, double str){
    buf_append(buf, (const uint8_t *)&str, sizeof(double));
}