#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstdlib>
#include <string>

const int k_max_load_factor=8;
const int k_rehashing_wwork=128;
//instrusive data structure
struct HNode{
    HNode *next;
    uint64_t hcode=0;
};

struct HTab{
    HNode **tab=NULL;//array of slots
    size_t size=0;
    size_t mask=0;
};

struct HMap{
    HTab older;
    HTab newer;
    size_t migrate_pos=0;
};


HNode* hm_lookup(HMap* hmap, HNode* key,bool (*eq)(HNode* ,HNode* ));
HNode* hm_delete(HMap* hmap, HNode* key,bool (*eq)(HNode* ,HNode* ));
void hm_insert(HMap* hmap, HNode* node);
void hm_clear(HMap* hmap);
size_t hm_size(HMap* hmap);
void hm_foreach(HMap* hmap,bool (*f)(HNode*,void*),void* arg);