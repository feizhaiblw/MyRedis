#include <cstdio>
#include <cassert>
#include <cstring>
#include <string>
#include "include/hashtable.h"

// 定义键值对结构体，用于测试
struct KeyValuePair {
    HNode node;  // 继承自HNode
    std::string key;
    std::string value;
};

// 比较两个节点是否相等
bool eq_str(HNode* a, HNode* b) {
    KeyValuePair* pa = reinterpret_cast<KeyValuePair*>(a);
    KeyValuePair* pb = reinterpret_cast<KeyValuePair*>(b);
    return pa->key == pb->key;
}

// 计算字符串的哈希值
uint64_t hash_str(const char* str) {
    uint64_t h = 0;
    while (*str) {
        h = h * 131 + *str++;
    }
    return h;
}

// 创建一个新的键值对节点
KeyValuePair* create_kv(const char* key, const char* value) {
    KeyValuePair* kv = new KeyValuePair();
    kv->key = key;
    kv->value = value;
    kv->node.hcode = hash_str(key);
    kv->node.next = NULL;
    return kv;
}

// 释放键值对节点
void free_kv(KeyValuePair* kv) {
    delete kv;
}

// 用于遍历的回调函数
bool print_kv(HNode* node, void* arg) {
    KeyValuePair* kv = reinterpret_cast<KeyValuePair*>(node);
    printf("  %s => %s\n", kv->key.c_str(), kv->value.c_str());
    return true; // 继续遍历
}

// 测试基本的插入、查找和删除操作
void test_basic_operations() {
    printf("\nRunning test_basic_operations...\n");
    HMap hmap = {};
    
    // 测试空表查找
    KeyValuePair dummy = {{}, "key1", ""};
    dummy.node.hcode = hash_str("key1");
    assert(hm_lookup(&hmap, &dummy.node, eq_str) == NULL);
    
    // 插入几个键值对
    KeyValuePair* kv1 = create_kv("key1", "value1");
    KeyValuePair* kv2 = create_kv("key2", "value2");
    KeyValuePair* kv3 = create_kv("key3", "value3");
    
    hm_insert(&hmap, &kv1->node);
    hm_insert(&hmap, &kv2->node);
    hm_insert(&hmap, &kv3->node);
    
    // 验证大小
    assert(hm_size(&hmap) == 3);
    
    // 查找已存在的键
    KeyValuePair search1 = {{}, "key1", ""};
    search1.node.hcode = hash_str("key1");
    HNode* found1 = hm_lookup(&hmap, &search1.node, eq_str);
    assert(found1 != NULL);
    assert(reinterpret_cast<KeyValuePair*>(found1)->value == "value1");
    
    // 查找不存在的键
    KeyValuePair search4 = {{}, "key4", ""};
    search4.node.hcode = hash_str("key4");
    assert(hm_lookup(&hmap, &search4.node, eq_str) == NULL);
    
    // 删除一个键
    HNode* deleted = hm_delete(&hmap, &search1.node, eq_str);
    assert(deleted != NULL);
    assert(hm_size(&hmap) == 2);
    assert(hm_lookup(&hmap, &search1.node, eq_str) == NULL);
    
    // 遍历剩余的键值对
    printf("Remaining key-value pairs after deletion:\n");
    hm_foreach(&hmap, print_kv, NULL);
    
    // 清理
    hm_clear(&hmap);
    free_kv(kv1);
    free_kv(kv2);
    free_kv(kv3);
    
    printf("✅ Test_basic_operations passed!\n");
}

// 测试重哈希机制
void test_rehashing() {
    printf("\nRunning test_rehashing...\n");
    HMap hmap = {};
    
    // 插入足够多的元素以触发重哈希
    const int NUM_ELEMENTS = 100;
    KeyValuePair* kvs[NUM_ELEMENTS];
    
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        char key[32], value[32];
        sprintf(key, "key%d", i);
        sprintf(value, "value%d", i);
        kvs[i] = create_kv(key, value);
        hm_insert(&hmap, &kvs[i]->node);
    }
    
    // 验证大小
    assert(hm_size(&hmap) == NUM_ELEMENTS);
    
    // 验证所有元素都能被正确查找
    int found_count = 0;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        char key[32];
        sprintf(key, "key%d", i);
        KeyValuePair search = {{}, key, ""};
        search.node.hcode = hash_str(key);
        HNode* found = hm_lookup(&hmap, &search.node, eq_str);
        if (found != NULL) {
            found_count++;
        }
    }
    assert(found_count == NUM_ELEMENTS);
    
    // 清理
    hm_clear(&hmap);
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        free_kv(kvs[i]);
    }
    
    printf("✅ Test_rehashing passed!\n");
}

// 测试遍历功能
void test_foreach() {
    printf("\nRunning test_foreach...\n");
    HMap hmap = {};
    
    // 插入一些元素
    KeyValuePair* kv1 = create_kv("name", "Redis");
    KeyValuePair* kv2 = create_kv("type", "Database");
    KeyValuePair* kv3 = create_kv("language", "C++");
    
    hm_insert(&hmap, &kv1->node);
    hm_insert(&hmap, &kv2->node);
    hm_insert(&hmap, &kv3->node);
    
    // 遍历并打印所有键值对
    printf("All key-value pairs in the hash table:\n");
    hm_foreach(&hmap, print_kv, NULL);
    
    // 使用自定义参数的遍历回调
    struct CountArg {
        int count;
        std::string prefix;
    } arg = {0, "key_"};
    
    auto count_prefix = [](HNode* node, void* arg_ptr) -> bool {
        CountArg* a = static_cast<CountArg*>(arg_ptr);
        KeyValuePair* kv = reinterpret_cast<KeyValuePair*>(node);
        if (kv->key.compare(0, a->prefix.length(), a->prefix) == 0) {
            a->count++;
        }
        return true;
    };
    
    hm_foreach(&hmap, count_prefix, &arg);
    assert(arg.count == 0); // 我们插入的键都不以"key_"开头
    
    // 清理
    hm_clear(&hmap);
    free_kv(kv1);
    free_kv(kv2);
    free_kv(kv3);
    
    printf("✅ Test_foreach passed!\n");
}

// 测试边界情况
void test_edge_cases() {
    printf("\nRunning test_edge_cases...\n");
    HMap hmap = {};
    
    // 测试空表的大小
    assert(hm_size(&hmap) == 0);
    
    // 测试空表的清除操作
    hm_clear(&hmap);
    assert(hmap.newer.tab == NULL);
    assert(hmap.older.tab == NULL);
    
    // 测试在同一键上的多次插入
    KeyValuePair* kv1 = create_kv("duplicate", "first");
    KeyValuePair* kv2 = create_kv("duplicate", "second");
    
    hm_insert(&hmap, &kv1->node);
    hm_insert(&hmap, &kv2->node);
    
    // 哈希表应该包含两个节点（相同键但不同对象）
    assert(hm_size(&hmap) == 2);
    
    // 测试对空表的删除操作
    HMap empty_hmap = {};
    KeyValuePair dummy = {{}, "dummy", ""};
    dummy.node.hcode = hash_str("dummy");
    assert(hm_delete(&empty_hmap, &dummy.node, eq_str) == NULL);
    
    // 清理
    hm_clear(&hmap);
    free_kv(kv1);
    free_kv(kv2);
    
    printf("✅ Test_edge_cases passed!\n");
}

// 测试哈希冲突处理
void test_hash_collision() {
    printf("\nRunning test_hash_collision...\n");
    HMap hmap = {};
    
    // 故意创建一些具有相同哈希码的键
    struct CollisionNode {
        HNode node;
        int id;
    };
    
    auto eq_collision = [](HNode* a, HNode* b) -> bool {
        return reinterpret_cast<CollisionNode*>(a)->id == 
               reinterpret_cast<CollisionNode*>(b)->id;
    };
    
    const int COLLISION_COUNT = 5;
    CollisionNode nodes[COLLISION_COUNT];
    uint64_t same_hash = hash_str("same");
    
    // 设置相同的哈希码但不同的ID
    for (int i = 0; i < COLLISION_COUNT; i++) {
        nodes[i].node.hcode = same_hash;
        nodes[i].node.next = NULL;
        nodes[i].id = i;
        hm_insert(&hmap, &nodes[i].node);
    }
    
    // 验证所有节点都被正确插入
    assert(hm_size(&hmap) == COLLISION_COUNT);
    
    // 查找每个节点
    for (int i = 0; i < COLLISION_COUNT; i++) {
        CollisionNode search_node;
        search_node.node.hcode = same_hash;
        search_node.id = i;
        HNode* found = hm_lookup(&hmap, &search_node.node, eq_collision);
        assert(found != NULL);
        assert(reinterpret_cast<CollisionNode*>(found)->id == i);
    }
    
    // 清理
    hm_clear(&hmap);
    
    printf("✅ Test_hash_collision passed!\n");
}

int main() {
    printf("====== Hashtable Complete Test Suite ======\n");
    
    test_basic_operations();
    test_rehashing();
    test_foreach();
    test_edge_cases();
    test_hash_collision();
    
    printf("\n✅ All hashtable tests passed successfully!\n");
    printf("==========================================\n");
    
    return 0;
}