#pragma once
#include <stdint.h>
#include <stddef.h>
#include <chrono>
#include <cstdint>

#define container_of(ptr,type,member)\
    ((type*)((char*)ptr - offsetof(type, member)))

//FNV-hash
static uint64_t str_hash(uint8_t* data, size_t len) {
    uint64_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

inline int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
