#pragma once
#include "hashtable.h"
#include "zset.h"
#include <string>
#include <cstdint>
#include <cstddef>
#include <pthread.h>

//value type
enum{
    T_INIT=0,
    T_SIR=1,  //string
    T_ZSET=2, //zset
};

struct Entry{
    HNode node;
    std::string key;
    uint32_t type=0;

    union {
        std::string str;
        ZSet zset;
    };

    Entry() : type(T_INIT) {}
    ~Entry() {
        if(type == T_SIR) {
            str.~basic_string();
        } else if(type == T_ZSET) {
            zset_clear(&zset);
            zset.~ZSet();
        }
    }

    bool has_expire=false;
    int64_t expire_at=0;
    size_t expire_heap_idx=static_cast<size_t>(-1);
};

Entry* entry_new(uint32_t type);
void entry_delete(Entry* entry);
bool entry_eq(HNode* a,HNode* b);

struct Data {
    HMap db;
};

extern Data g_data;

void db_clear_expire(Entry* entry);
void db_set_expire(Entry* entry, int64_t expire_ms);
bool db_expire_if_needed(Entry* entry, int64_t now_ms);
void db_purge_expired(int64_t now_ms);
int db_time_until_next_expire(int64_t now_ms);
void db_reset();

extern pthread_mutex_t g_db_mutex;
