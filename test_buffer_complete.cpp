#include <cstdio>
#include <cstring>
#include <cassert>
#include "include/buffer.h"

// 测试函数宏，简化测试代码
#define TEST(name) void test_##name()
#define RUN_TEST(name) printf("Running test_"#name"...\n"); test_##name(); printf("✅ Test_"#name" passed!\n\n")

// 验证缓冲区状态的辅助函数
void verify_buffer_state(Buffer* buf, size_t expected_data_size, size_t expected_capacity) {
    size_t actual_data_size = buf->data_end - buf->data_begin;
    size_t actual_capacity = buf->buffer_end - buf->buffer_begin;
    
    assert(actual_data_size == expected_data_size);
    assert(actual_capacity >= expected_capacity); // 可能会更大，因为扩容策略
    assert(buf->data_begin >= buf->buffer_begin);
    assert(buf->data_end <= buf->buffer_end);
    assert(buf->data_begin <= buf->data_end);
}

// 测试基本的添加和消费功能
TEST(basic_append_consume) {
    Buffer buf(1024); // 使用较小的初始容量便于测试
    
    // 验证初始状态
    verify_buffer_state(&buf, 0, 1024);
    
    // 添加一些数据
    const char* test_data = "Hello, Buffer!";
    size_t test_data_len = strlen(test_data);
    buf_append(&buf, (const uint8_t*)test_data, test_data_len);
    
    // 验证添加后状态
    verify_buffer_state(&buf, test_data_len, 1024);
    assert(memcmp(buf.data_begin, test_data, test_data_len) == 0);
    
    // 消费部分数据
    size_t consume_len = 7; // "Hello, "
    buf_consume(&buf, consume_len);
    verify_buffer_state(&buf, test_data_len - consume_len, 1024);
    assert(memcmp(buf.data_begin, test_data + consume_len, test_data_len - consume_len) == 0);
    
    // 消费剩余数据
    buf_consume(&buf, test_data_len - consume_len);
    verify_buffer_state(&buf, 0, 1024);
    assert(buf.data_begin == buf.buffer_begin); // 应该重置到起始位置
    assert(buf.data_end == buf.buffer_begin);
}

// 测试不同数据类型的添加
TEST(different_data_types) {
    Buffer buf(1024);
    
    // 测试uint8_t
    uint8_t u8_val = 0x42;
    buf_append_u8(&buf, u8_val);
    verify_buffer_state(&buf, sizeof(uint8_t), 1024);
    assert(*(uint8_t*)buf.data_begin == u8_val);
    buf_consume(&buf, sizeof(uint8_t));
    
    // 测试uint32_t
    uint32_t u32_val = 0x12345678;
    buf_append_u32(&buf, u32_val);
    verify_buffer_state(&buf, sizeof(uint32_t), 1024);
    assert(*(uint32_t*)buf.data_begin == u32_val);
    buf_consume(&buf, sizeof(uint32_t));
    
    // 测试uint64_t
    uint64_t u64_val = 0x1234567890ABCDEF;
    buf_append_i64(&buf, u64_val);
    verify_buffer_state(&buf, sizeof(uint64_t), 1024);
    assert(*(uint64_t*)buf.data_begin == u64_val);
    buf_consume(&buf, sizeof(uint64_t));
    
    // 测试double
    double dbl_val = 3.14159265359;
    buf_append_dbl(&buf, dbl_val);
    verify_buffer_state(&buf, sizeof(double), 1024);
    assert(*(double*)buf.data_begin == dbl_val);
    buf_consume(&buf, sizeof(double));
}

// 测试缓冲区扩容
TEST(buffer_resize) {
    // 使用小容量创建缓冲区，确保会触发扩容
    Buffer buf(32); // 32字节的小缓冲区
    
    // 添加足够多的数据，触发扩容
    const size_t large_data_size = 100; // 大于初始容量
    uint8_t* large_data = new uint8_t[large_data_size];
    for (size_t i = 0; i < large_data_size; i++) {
        large_data[i] = static_cast<uint8_t>(i);
    }
    
    buf_append(&buf, large_data, large_data_size);
    
    // 验证扩容后的数据正确性
    verify_buffer_state(&buf, large_data_size, large_data_size);
    assert(memcmp(buf.data_begin, large_data, large_data_size) == 0);
    
    delete[] large_data;
}

// 测试内存重排（当剩余空间足够但需要移动数据时）
TEST(buffer_rearrange) {
    // 使用更小的缓冲区容量，更容易触发重排
    Buffer buf(40); // 40字节缓冲区
    
    // 先添加一些数据，占用缓冲区的大部分空间
    const char* part1 = "12345678901234567890";
    buf_append(&buf, (const uint8_t*)part1, strlen(part1));
    
    // 消费部分数据，但不是全部
    size_t consume_len = 10; // 消费前10个字符
    buf_consume(&buf, consume_len);
    
    // 此时，数据在缓冲区中间位置
    assert(buf.data_begin > buf.buffer_begin);
    
    // 保存当前的数据大小和剩余空间
    size_t remaining_space = buf.buffer_end - buf.data_end;
    size_t data_size = buf.data_end - buf.data_begin;
    
    // 保存当前的缓冲区容量，用于后面的断言
    size_t original_capacity = buf.buffer_end - buf.buffer_begin;
    
    // 计算一个刚好会导致重排的数据大小
    // 我们需要确保：remaining_space < part2_len <= (original_capacity - data_size)
    size_t part2_len = remaining_space + 5; // 比剩余空间大5字节
    
    // 动态创建测试数据
    uint8_t* part2 = new uint8_t[part2_len];
    for (size_t i = 0; i < part2_len; i++) {
        part2[i] = static_cast<uint8_t>(i + 65); // 填充字母A开始的ASCII字符
    }
    
    // 添加数据，应该触发重排而不是扩容
    buf_append(&buf, part2, part2_len);
    
    // 验证数据位置是否重置到缓冲区起始位置
    assert(buf.data_begin == buf.buffer_begin);
    
    // 验证缓冲区容量没有改变（没有触发扩容）
    assert(buf.buffer_end - buf.buffer_begin == original_capacity);
    
    // 验证数据正确性
    assert(memcmp(buf.data_begin, part1 + consume_len, data_size) == 0);
    assert(memcmp(buf.data_begin + data_size, part2, part2_len) == 0);
    
    // 清理动态分配的内存
    delete[] part2;
}

// 测试边界情况
TEST(edge_cases) {
    // 测试消费0字节
    Buffer buf(1024);
    const char* test_data = "Edge case test";
    buf_append(&buf, (const uint8_t*)test_data, strlen(test_data));
    
    // 消费0字节不应该有任何影响
    buf_consume(&buf, 0);
    verify_buffer_state(&buf, strlen(test_data), 1024);
    assert(memcmp(buf.data_begin, test_data, strlen(test_data)) == 0);
    
    // 测试添加0字节
    Buffer buf2(1024);
    buf_append(&buf2, (const uint8_t*)"", 0);
    verify_buffer_state(&buf2, 0, 1024);
    
    // 测试空缓冲区消费
    Buffer buf3(1024);
    buf_consume(&buf3, 10); // 尝试消费不存在的数据
    verify_buffer_state(&buf3, 0, 1024); // 应该保持不变
}

// 测试连续操作
TEST(consecutive_operations) {
    Buffer buf(1024);
    
    // 连续添加和消费操作的组合
    const int iterations = 100;
    int consumed_count = 0; // 记录已消费的值数量
    
    for (int i = 0; i < iterations; i++) {
        // 添加数据
        uint32_t val = static_cast<uint32_t>(i);
        printf("Adding value: %u at i=%d\n", val, i);
        buf_append_u32(&buf, val);
        
        // 每10次迭代消费一次
        if (i % 10 == 9) {
            // 消费前面5个值（从已消费数量开始）
            printf("\nConsuming at i=%d:\n", i);
            for (int k = 0; k < 5; k++) {
                int j = consumed_count + k;
                uint32_t read_val;
                memcpy(&read_val, buf.data_begin, sizeof(uint32_t));
                printf("  Expect: %u, Read: %u\n", static_cast<uint32_t>(j), read_val);
                assert(read_val == static_cast<uint32_t>(j));
                buf_consume(&buf, sizeof(uint32_t));
            }
            consumed_count += 5; // 一次性更新已消费数量
            printf("\n");
        }
    }
    
    // 消费剩余的所有数据
    size_t remaining_data = buf.data_end - buf.data_begin;
    assert(remaining_data % sizeof(uint32_t) == 0);
    size_t remaining_values = remaining_data / sizeof(uint32_t);
    
    printf("\nConsuming remaining values:\n");
    for (int i = consumed_count; i < iterations; i++) {
        uint32_t read_val;
        memcpy(&read_val, buf.data_begin, sizeof(uint32_t));
        printf("  Expect: %u, Read: %u\n", static_cast<uint32_t>(i), read_val);
        assert(read_val == static_cast<uint32_t>(i));
        buf_consume(&buf, sizeof(uint32_t));
    }
    
    // 验证缓冲区为空
    verify_buffer_state(&buf, 0, 1024);
    printf("\n✅ Test_consecutive_operations passed!\n");
}

int main() {
    printf("====== Buffer Complete Test Suite ======\n\n");
    
    RUN_TEST(basic_append_consume);
    RUN_TEST(different_data_types);
    RUN_TEST(buffer_resize);
    RUN_TEST(buffer_rearrange);
    RUN_TEST(edge_cases);
    RUN_TEST(consecutive_operations);
    
    printf("\n✅ All buffer tests passed successfully!\n");
    printf("=======================================\n");
    
    return 0;
}