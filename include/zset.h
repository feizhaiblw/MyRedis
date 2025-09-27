#pragma once

#include "avltree.h"
#include "hashtable.h"

struct ZSet {
    AVLNode* root=nullptr;
    HMap hmap;
};

struct ZNode{
    AVLNode tree;
    HNode hmap;
    double score=0;
    size_t len=0;
    char name[0];
};

bool zset_insert(ZSet* zset, const char* name, size_t len, double score);
ZNode* zset_lookup(ZSet* zset, const char* name, size_t len);
void zset_delete(ZSet* zset, ZNode* node);
ZNode* zset_lookge(ZSet* zset, double score,const char* name, size_t len);
void zset_clear(ZSet* zset);
ZNode* znode_offset(ZNode* node, int64_t offset);

size_t zset_rank(ZSet* zset, const char* name, size_t len);
ZNode* zset_nth(ZSet* zset, size_t rank);
size_t zset_card(ZSet* zset);
int zset_range(ZSet* zset, size_t start, size_t count, ZNode** results);
int zset_range_by_score(ZSet* zset, double min_score, double max_score, ZNode** results, size_t max_results);
