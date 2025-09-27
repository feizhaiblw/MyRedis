#include "../include/db.h"
#include "../include/common.h"
#include "../include/minheap.h"
#include <cstring>
#include <limits>
#include <vector>

Data g_data;
static ExpireMinHeap g_expire_heap;
pthread_mutex_t g_db_mutex = PTHREAD_MUTEX_INITIALIZER;

Entry* entry_new(uint32_t type){
    Entry* entry=new Entry();
    entry->type=type;
    if(type == T_SIR) {
        new(&entry->str) std::string();
    } else if(type == T_ZSET) {
        new(&entry->zset) ZSet();
    }
    return entry;
}

void entry_delete(Entry* entry){
    if(!entry){
        return;
    }
    db_clear_expire(entry);
    delete entry;
}

bool entry_eq(HNode* a,HNode* b){
    Entry* le=container_of(a,Entry,node);
    Entry* re=container_of(b,Entry,node);
    return le->key==re->key;
}

void db_clear_expire(Entry* entry){
    if(!entry || !entry->has_expire){
        return;
    }
    g_expire_heap.remove(entry);
    entry->has_expire = false;
    entry->expire_at = 0;
    entry->expire_heap_idx = static_cast<size_t>(-1);
}

void db_set_expire(Entry* entry, int64_t expire_ms){
    if(!entry){
        return;
    }
    entry->has_expire = true;
    entry->expire_at = expire_ms;
    g_expire_heap.add_or_update(entry, expire_ms);
}

bool db_expire_if_needed(Entry* entry, int64_t now_ms){
    if(!entry || !entry->has_expire){
        return false;
    }
    if(entry->expire_at > now_ms){
        return false;
    }
    db_clear_expire(entry);
    hm_delete(&g_data.db, &entry->node, entry_eq);
    entry_delete(entry);
    return true;
}

void db_purge_expired(int64_t now_ms){
    while(!g_expire_heap.empty()){
        HeapItem* head = g_expire_heap.peek();
        if(!head || head->expire_at > now_ms){
            break;
        }
        HeapItem item = g_expire_heap.pop();
        Entry* entry = item.entry;
        if(!entry){
            continue;
        }
        entry->has_expire = false;
        entry->expire_at = 0;
        entry->expire_heap_idx = static_cast<size_t>(-1);
        hm_delete(&g_data.db, &entry->node, entry_eq);
        entry_delete(entry);
    }
}

int db_time_until_next_expire(int64_t now_ms){
    const HeapItem* head = g_expire_heap.peek();
    if(!head){
        return -1;
    }
    int64_t diff = head->expire_at - now_ms;
    if(diff <= 0){
        return 0;
    }
    if(diff > std::numeric_limits<int>::max()){
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(diff);
}

static bool collect_entries(HNode* node, void* arg){
    auto entries = static_cast<std::vector<Entry*>*>(arg);
    entries->push_back(container_of(node, Entry, node));
    return true;
}

void db_reset(){
    std::vector<Entry*> entries;
    hm_foreach(&g_data.db, collect_entries, &entries);
    for(Entry* entry : entries){
        hm_delete(&g_data.db, &entry->node, entry_eq);
        entry_delete(entry);
    }
    hm_clear(&g_data.db);
}
