#pragma once
#include <cstdint>
#include <cstddef>

const int MAX_MASSEGE = 32<<20;
//optimize buffer via pointer instead of vector
struct Buffer {
    uint8_t *buffer_begin;
    uint8_t *buffer_end;
    uint8_t *data_begin;
    uint8_t *data_end;
    Buffer(size_t capacity = MAX_MASSEGE) {
        buffer_begin = new uint8_t[capacity];
        buffer_end = buffer_begin + capacity;
        data_begin = buffer_begin;
        data_end = buffer_begin;
    }
    ~Buffer() { delete[] buffer_begin; }
};

void buf_consume(Buffer* buf, size_t n);
void buf_append(Buffer* buf, const uint8_t* data, size_t len);
void buf_append_u8(Buffer* buf, uint8_t data);
void buf_append_u32(Buffer* buf, uint32_t data);
void buf_append_i64(Buffer* buf, uint64_t data);
void buf_append_dbl(Buffer* buf, double str);