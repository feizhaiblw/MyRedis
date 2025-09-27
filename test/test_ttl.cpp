#include "include/commands.h"
#include "include/common.h"
#include "include/db.h"
#include "include/response.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>

struct DbLock {
    DbLock(){ pthread_mutex_lock(&g_db_mutex); }
    ~DbLock(){ pthread_mutex_unlock(&g_db_mutex); }
};

static Entry* find_entry(const std::string& key) {
    DbLock lock;
    Entry probe;
    probe.key = key;
    probe.node.hcode = str_hash(reinterpret_cast<uint8_t*>(const_cast<char*>(key.data())), key.size());
    HNode* node = hm_lookup(&g_data.db, &probe.node, entry_eq);
    if (!node) {
        return nullptr;
    }
    return container_of(node, Entry, node);
}

static void expect_tag(const Buffer& buf, uint8_t tag) {
    size_t len = static_cast<size_t>(buf.data_end - buf.data_begin);
    assert(len >= 1);
    assert(buf.data_begin[0] == tag);
}

static int64_t read_int(const Buffer& buf) {
    expect_tag(buf, TAG_INT);
    int64_t value = 0;
    std::memcpy(&value, buf.data_begin + 1, sizeof(value));
    return value;
}

static std::string read_bulk_string(const Buffer& buf) {
    const uint8_t* ptr = buf.data_begin;
    size_t len = static_cast<size_t>(buf.data_end - buf.data_begin);
    assert(len >= 4 + 1 + 4);
    uint32_t prefix_len = 0;
    std::memcpy(&prefix_len, ptr, sizeof(prefix_len));
    ptr += sizeof(prefix_len);
    uint8_t tag = *ptr++;
    assert(tag == TAG_STR);
    uint32_t str_len = 0;
    std::memcpy(&str_len, ptr, sizeof(str_len));
    ptr += sizeof(str_len);
    assert(len >= 4 + 1 + 4 + str_len);
    return std::string(reinterpret_cast<const char*>(ptr), str_len);
}

static void test_setex_basic() {
    {
        DbLock lock;
        db_reset();
    }
    std::vector<std::string> cmd = {"setex", "foo", "1", "bar"};
    Buffer out;
    {
        DbLock lock;
        do_setex(cmd, out);
    }
    expect_tag(out, TAG_NIL);

    Entry* entry = find_entry("foo");
    assert(entry != nullptr);
    assert(entry->has_expire);
    int64_t expire_at = entry->expire_at;
    int64_t now = now_ms();
    assert(expire_at > now);

    Buffer get_out;
    std::vector<std::string> get_cmd = {"get", "foo"};
    {
        DbLock lock;
        do_get(get_cmd, get_out);
    }
    std::string val = read_bulk_string(get_out);
    assert(val == "bar");

    {
        DbLock lock;
        db_purge_expired(expire_at + 1);
    }
    entry = find_entry("foo");
    assert(entry == nullptr);

    Buffer get_after_out;
    {
        DbLock lock;
        do_get(get_cmd, get_after_out);
    }
    expect_tag(get_after_out, TAG_NIL);
}

static void test_expire_immediate() {
    {
        DbLock lock;
        db_reset();
    }
    std::vector<std::string> set_cmd = {"set", "key", "value"};
    Buffer set_out;
    {
        DbLock lock;
        do_set(set_cmd, set_out);
    }
    expect_tag(set_out, TAG_NIL);

    std::vector<std::string> expire_cmd = {"expire", "key", "0"};
    Buffer expire_out;
    {
        DbLock lock;
        do_expire(expire_cmd, expire_out);
    }
    expect_tag(expire_out, TAG_INT);
    assert(read_int(expire_out) == 1);

    Entry* entry = find_entry("key");
    assert(entry == nullptr);

    std::vector<std::string> get_cmd = {"get", "key"};
    Buffer get_out;
    {
        DbLock lock;
        do_get(get_cmd, get_out);
    }
    expect_tag(get_out, TAG_NIL);
}

static void test_expire_missing() {
    {
        DbLock lock;
        db_reset();
    }
    std::vector<std::string> expire_cmd = {"expire", "missing", "10"};
    Buffer expire_out;
    {
        DbLock lock;
        do_expire(expire_cmd, expire_out);
    }
    expect_tag(expire_out, TAG_INT);
    assert(read_int(expire_out) == 0);
}

int main() {
    test_setex_basic();
    test_expire_immediate();
    test_expire_missing();
    std::cout << "All TTL tests passed\n";
    return 0;
}
