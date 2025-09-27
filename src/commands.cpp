#include "../include/commands.h"
#include "../include/common.h"

#include <cstdlib>
#include <cerrno>
#include <limits>

static uint64_t hash_key(const std::string& key){
    return str_hash(reinterpret_cast<uint8_t*>(const_cast<char*>(key.data())), key.size());
}

static void init_probe(Entry& probe, const std::string& key){
    probe.key = key;
    probe.node.hcode = hash_key(key);
}

static Entry* lookup_entry(const std::string& key){
    Entry probe;
    init_probe(probe, key);
    HNode* node = hm_lookup(&g_data.db, &probe.node, entry_eq);
    if(!node){
        return nullptr;
    }
    return container_of(node, Entry, node);
}

static Entry* detach_entry(const std::string& key){
    Entry probe;
    init_probe(probe, key);
    HNode* node = hm_delete(&g_data.db, &probe.node, entry_eq);
    if(!node){
        return nullptr;
    }
    return container_of(node, Entry, node);
}

static bool parse_integer(const std::string& text, int64_t& out){
    errno = 0;
    char* end = nullptr;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if(errno != 0 || end == text.c_str() || *end != '\0'){
        return false;
    }
    out = static_cast<int64_t>(value);
    return true;
}

static bool ensure_string_entry(Entry* entry, Buffer& out){
    if(!entry || entry->type != T_SIR){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return false;
    }
    return true;
}

static bool maybe_expire_and_detach(Entry*& entry, int64_t now){
    if(!entry){
        return false;
    }
    if(db_expire_if_needed(entry, now)){
        entry = nullptr;
        return true;
    }
    return false;
}

static bool normalize_ttl_seconds(const std::string& arg, int64_t now_ms, int64_t& out_expire_ms){
    int64_t seconds = 0;
    if(!parse_integer(arg, seconds)){
        return false;
    }
    if(seconds <= 0){
        out_expire_ms = now_ms;
        return true;
    }
    constexpr int64_t kMillisPerSec = 1000;
    if(seconds > (std::numeric_limits<int64_t>::max() / kMillisPerSec)){
        out_expire_ms = std::numeric_limits<int64_t>::max();
        return true;
    }
    out_expire_ms = now_ms + seconds * kMillisPerSec;
    if(out_expire_ms < 0){
        out_expire_ms = std::numeric_limits<int64_t>::max();
    }
    return true;
}

static bool cb_keys(HNode* node,void* arg){
    Buffer& out = *(Buffer*)arg;
    const std::string &key = container_of(node,Entry,node)->key;
    out_str(out, key.data(),key.size());
    return true;
}

void do_keys(std::vector<std::string>&, Buffer& out){
    db_purge_expired(now_ms());
    out_arr(out, hm_size(&g_data.db));
    hm_foreach(&g_data.db, &cb_keys, (void*)&out);
}

void do_get(std::vector<std::string>& cmd, Buffer& out){
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_nil(out);
        return;
    }
    if(!ensure_string_entry(entry, out)){
        return;
    }
    const std::string& val = entry->str;
    buf_append_u32(&out, val.size());
    out_str(out, val.data(), val.size());
}

void do_set(std::vector<std::string>& cmd, Buffer& out){
    int64_t now = now_ms();
    const std::string& key = cmd[1];
    Entry* entry = lookup_entry(key);
    if(maybe_expire_and_detach(entry, now)){
        entry = nullptr;
    }
    if(entry){
        if(!ensure_string_entry(entry, out)){
            return;
        }
        entry->str = cmd[2];
        db_clear_expire(entry);
    }else{
        entry = entry_new(T_SIR);
        entry->key = key;
        entry->node.hcode = hash_key(key);
        entry->str = cmd[2];
        hm_insert(&g_data.db, &entry->node);
    }
    out_nil(out);
}

void do_setex(std::vector<std::string>& cmd, Buffer& out){
    int64_t now = now_ms();
    int64_t expire_ms = 0;
    if(!normalize_ttl_seconds(cmd[2], now, expire_ms)){
        out_err(out, ERROR, "invalid expire time");
        return;
    }
    const std::string& key = cmd[1];
    Entry* entry = lookup_entry(key);
    if(maybe_expire_and_detach(entry, now)){
        entry = nullptr;
    }
    if(entry){
        if(!ensure_string_entry(entry, out)){
            return;
        }
        entry->str = cmd[3];
    }else{
        entry = entry_new(T_SIR);
        entry->key = key;
        entry->node.hcode = hash_key(key);
        entry->str = cmd[3];
        hm_insert(&g_data.db, &entry->node);
    }
    if(expire_ms <= now){
        Entry* removed = detach_entry(key);
        if(removed){
            entry_delete(removed);
        }
        out_nil(out);
        return;
    }
    db_set_expire(entry, expire_ms);
    out_nil(out);
}

void do_expire(std::vector<std::string>& cmd, Buffer& out){
    int64_t now = now_ms();
    int64_t expire_ms = 0;
    if(!normalize_ttl_seconds(cmd[2], now, expire_ms)){
        out_err(out, ERROR, "invalid expire time");
        return;
    }
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_int(out, 0);
        return;
    }
    if(expire_ms <= now){
        Entry* removed = detach_entry(cmd[1]);
        if(removed){
            entry_delete(removed);
            out_int(out, 1);
        }else{
            out_int(out, 0);
        }
        return;
    }
    db_set_expire(entry, expire_ms);
    out_int(out, 1);
}

void do_del(std::vector<std::string>& cmd, Buffer& out){
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_int(out, 0);
        return;
    }
    Entry* removed = detach_entry(cmd[1]);
    if(removed){
        entry_delete(removed);
        out_int(out, 1);
    }else{
        out_int(out, 0);
    }
}

void do_zadd(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() % 2 != 0 || cmd.size() < 4) {
        out_err(out, ERROR, "ZADD key score member [score member ...]");
        return;
    }
    int64_t now = now_ms();
    const std::string& key = cmd[1];
    Entry* entry = lookup_entry(key);
    if(maybe_expire_and_detach(entry, now)){
        entry = nullptr;
    }
    if(!entry){
        entry = entry_new(T_ZSET);
        entry->key = key;
        entry->node.hcode = hash_key(key);
        hm_insert(&g_data.db, &entry->node);
    }else if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    int added = 0;
    for(size_t i = 2; i < cmd.size(); i += 2) {
        double score = atof(cmd[i].c_str());
        const std::string& member = cmd[i + 1];
        if(zset_insert(&entry->zset, member.c_str(), member.size(), score)) {
            added++;
        }
    }
    out_int(out, added);
}

void do_zrem(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() < 3) {
        out_err(out, ERROR, "ZREM key member [member ...]");
        return;
    }
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_int(out, 0);
        return;
    }
    if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    int removed = 0;
    for(size_t i = 2; i < cmd.size(); i++) {
        ZNode* znode = zset_lookup(&entry->zset, cmd[i].c_str(), cmd[i].size());
        if(znode) {
            zset_delete(&entry->zset, znode);
            removed++;
        }
    }
    out_int(out, removed);
}

void do_zscore(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() != 3) {
        out_err(out, ERROR, "ZSCORE key member");
        return;
    }
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_nil(out);
        return;
    }
    if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    ZNode* znode = zset_lookup(&entry->zset, cmd[2].c_str(), cmd[2].size());
    if(!znode) {
        out_nil(out);
        return;
    }
    out_dbl(out, znode->score);
}

void do_zrank(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() != 3) {
        out_err(out, ERROR, "ZRANK key member");
        return;
    }
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_nil(out);
        return;
    }
    if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    size_t rank = zset_rank(&entry->zset, cmd[2].c_str(), cmd[2].size());
    if(rank == 0) {
        out_nil(out);
        return;
    }
    out_int(out, static_cast<int64_t>(rank) - 1);
}

void do_zcard(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() != 2) {
        out_err(out, ERROR, "ZCARD key");
        return;
    }
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_int(out, 0);
        return;
    }
    if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    out_int(out, zset_card(&entry->zset));
}

void do_zrange(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() < 4 || cmd.size() > 5) {
        out_err(out, ERROR, "ZRANGE key start stop [WITHSCORES]");
        return;
    }
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_arr(out, 0);
        return;
    }
    if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    int start = atoi(cmd[2].c_str());
    int stop = atoi(cmd[3].c_str());
    bool with_scores = (cmd.size() == 5 && cmd[4] == "WITHSCORES");
    size_t card = zset_card(&entry->zset);
    if(card == 0) {
        out_arr(out, 0);
        return;
    }
    if(start < 0) start += static_cast<int>(card);
    if(stop < 0) stop += static_cast<int>(card);
    if(start < 0) start = 0;
    if(stop >= static_cast<int>(card)) stop = static_cast<int>(card) - 1;
    if(start > stop) {
        out_arr(out, 0);
        return;
    }
    size_t count = static_cast<size_t>(stop - start + 1);
    ZNode** results = (ZNode**)malloc(count * sizeof(ZNode*));
    int actual_count = zset_range(&entry->zset, start, count, results);
    out_arr(out, with_scores ? actual_count * 2 : actual_count);
    for(int i = 0; i < actual_count; i++) {
        out_str(out, results[i]->name, results[i]->len);
        if(with_scores) {
            out_dbl(out, results[i]->score);
        }
    }
    free(results);
}

void do_zrangebyscore(std::vector<std::string>& cmd, Buffer& out){
    if(cmd.size() < 4 || cmd.size() > 5) {
        out_err(out, ERROR, "ZRANGEBYSCORE key min max [WITHSCORES]");
        return;
    }
    int64_t now = now_ms();
    Entry* entry = lookup_entry(cmd[1]);
    if(maybe_expire_and_detach(entry, now) || !entry){
        out_arr(out, 0);
        return;
    }
    if(entry->type != T_ZSET){
        out_err(out, ERROR, "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    double min_score = atof(cmd[2].c_str());
    double max_score = atof(cmd[3].c_str());
    bool with_scores = (cmd.size() == 5 && cmd[4] == "WITHSCORES");
    size_t max_results = 1000;
    ZNode** results = (ZNode**)malloc(max_results * sizeof(ZNode*));
    int actual_count = zset_range_by_score(&entry->zset, min_score, max_score, results, max_results);
    out_arr(out, with_scores ? actual_count * 2 : actual_count);
    for(int i = 0; i < actual_count; i++) {
        out_str(out, results[i]->name, results[i]->len);
        if(with_scores) {
            out_dbl(out, results[i]->score);
        }
    }
    free(results);
}

void do_request(std::vector<std::string>& cmd, Buffer& out) {
    if(cmd.empty()) return;
    const std::string& op = cmd[0];
    if(op == "set" && cmd.size() == 3){
        do_set(cmd, out);
    }else if(op == "setex" && cmd.size() == 4){
        do_setex(cmd, out);
    }else if(op == "get" && cmd.size() == 2){
        do_get(cmd, out);
    }else if(op == "del" && cmd.size() == 2){
        do_del(cmd, out);
    }else if(op == "expire" && cmd.size() == 3){
        do_expire(cmd, out);
    }else if(op == "keys" && cmd.size() == 1){
        do_keys(cmd, out);
    }else if(op == "zadd"){
        do_zadd(cmd, out);
    }else if(op == "zrem"){
        do_zrem(cmd, out);
    }else if(op == "zscore"){
        do_zscore(cmd, out);
    }else if(op == "zrank"){
        do_zrank(cmd, out);
    }else if(op == "zcard"){
        do_zcard(cmd, out);
    }else if(op == "zrange"){
        do_zrange(cmd, out);
    }else if(op == "zrangebyscore"){
        do_zrangebyscore(cmd, out);
    }else{
        out_err(out, ERROR, "Unknown command");
    }
}
