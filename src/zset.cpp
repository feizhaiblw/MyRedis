#include <cstring>
#include "../include/zset.h"
#include "../include/avltree.h"
#include "../include/common.h"


static ZNode* znode_new(const char* name,size_t len,double score){ 
    ZNode* node=(ZNode*)malloc(sizeof(ZNode)+len);
    assert(node);
    avl_node_init(&node->tree);
    node->hmap.hcode=str_hash((uint8_t*)name,len);
    node->hmap.next=NULL;
    node->len=len;
    memcpy(node->name,name,len);
    node->score=score;
    return node;
}

static void znode_del(ZNode* node){
    free(node);
}
static size_t min(size_t a,size_t b){
    return a<b?a:b;
}

static bool zless(AVLNode* node1,double score,const char* name,size_t len){
    ZNode* znode1=container_of(node1,ZNode,tree);
    if(znode1->score!=score){
        return znode1->score<score;
    }
    int rv=memcmp(znode1->name,name,min(znode1->len,len));
    if(rv!=0){
        return rv<0;
    }
    return znode1->len<len;
}

static bool zless(AVLNode* node1,AVLNode* node2){
    ZNode* znode2=container_of(node2,ZNode,tree);
    return zless(node1,znode2->score,znode2->name,znode2->len);
}

static void tree_insert(ZSet* zset,ZNode* node){
    zset->root = avl_insert(zset->root, &node->tree, zless);
}

//update the score of node
static void zset_update(ZSet* zset,ZNode* node,double score){
    if(node->score==score){
        return;
    }

    // Simple approach: always remove and re-insert
    zset->root = avl_del(&node->tree);
    avl_node_init(&node->tree);
    node->score = score;
    tree_insert(zset, node);
}

struct HKey{
    HNode node;
    const char* name=nullptr;
    size_t len=0;
};

static bool hcmp(HNode* node,HNode* key){
    ZNode* znode=container_of(node,ZNode,hmap);
    HKey* hkey=container_of(key,HKey,node);
    if(znode->len!=hkey->len){
        return false;
    }
    return 0==memcmp(znode->name,hkey->name,znode->len);
}

ZNode* zset_lookup(ZSet* zset, const char* name, size_t len){
    if(!zset->root){
        return nullptr;
    }
    HKey hkey;
    hkey.name=name;
    hkey.len=len;
    hkey.node.hcode=str_hash((uint8_t*)name,len);
    HNode* found=hm_lookup(&zset->hmap,&hkey.node,&hcmp);
    return found?container_of(found,ZNode,hmap):nullptr;
}

bool zset_insert(ZSet* zset, const char* name, size_t len, double score){
    ZNode* node=zset_lookup(zset,name,len);
    if(node){
        zset_update(zset,node,score);
        return false;
    }
    else{
        ZNode* node=znode_new(name,len,score);
        hm_insert(&zset->hmap,&node->hmap);
        tree_insert(zset,node);
        return true;
    }
}

//find the first (score,name) tuple that is >= key
ZNode* zset_lookge(ZSet* zset, double score,const char* name, size_t len){
    AVLNode* found=NULL;
    for(AVLNode* node=zset->root;node;){
        if(zless(node,score,name,len)){
            node=node->right;
        }
        else{
            found=node;
            node=node->left;
        }
    }
    return found?container_of(found,ZNode,tree):NULL;
}

void zset_delete(ZSet* zset, ZNode* node){
    if (!zset || !node) return;

    // Remove from hashtable first
    HKey hkey;
    hkey.len=node->len;
    hkey.name=node->name;
    hkey.node.hcode=node->hmap.hcode;
    HNode* found=hm_delete(&zset->hmap,&hkey.node,&hcmp);

    if (!found) {
        // Node not found in hashtable, something is wrong
        return;
    }

    // Remove from tree
    zset->root = avl_del(&node->tree);

    // Free the node
    znode_del(node);
}

//offset into the succeeding or preceding node.
ZNode* znode_offset(ZNode* node, int64_t offset){
    AVLNode* tnode=node?avl_offset(&node->tree,offset):nullptr;
    return tnode?container_of(tnode,ZNode,tree):NULL;
}

static void tree_dispose(AVLNode* node){ 
    if(!node){
        return;
    }
    tree_dispose(node->left);
    tree_dispose(node->right);
    znode_del(container_of(node,ZNode,tree));
}

void zset_clear(ZSet* zset){
    if (!zset) return;

    // First dispose of the tree nodes (this frees the actual node memory)
    tree_dispose(zset->root);
    zset->root = nullptr;

    // Then clear hashtable (this only clears the bucket arrays)
    hm_clear(&zset->hmap);
}

size_t zset_rank(ZSet* zset, const char* name, size_t len){
    if (!zset || !zset->root) return 0;

    ZNode* node = zset_lookup(zset, name, len);
    if (!node) return 0;

    size_t rank = 0;
    AVLNode* current = &node->tree;

    // Add size of left subtree
    rank += avl_size(current->left);

    // Traverse up to root, adding sizes of left subtrees when coming from right
    while (current->parent) {
        AVLNode* parent = current->parent;
        if (parent->right == current) {
            // Coming from right child, add left subtree + parent
            rank += avl_size(parent->left) + 1;
        }
        current = parent;
    }

    return rank + 1; // 1-based ranking
}

ZNode* zset_nth(ZSet* zset, size_t rank){
    if (!zset || !zset->root || rank == 0) return nullptr;

    AVLNode* current = zset->root;
    size_t current_rank = rank;

    while (current) {
        size_t left_size = avl_size(current->left);

        if (current_rank == left_size + 1) {
            // Found the target node
            return container_of(current, ZNode, tree);
        } else if (current_rank <= left_size) {
            // Target is in left subtree
            current = current->left;
        } else {
            // Target is in right subtree
            current_rank -= (left_size + 1);
            current = current->right;
        }
    }

    return nullptr;
}

size_t zset_card(ZSet* zset){
    if (!zset || !zset->root) return 0;
    return avl_size(zset->root);
}

int zset_range(ZSet* zset, size_t start, size_t count, ZNode** results){
    if (!zset || !results || count == 0) return 0;

    size_t collected = 0;
    for (size_t i = 0; i < count && collected < count; i++) {
        ZNode* node = zset_nth(zset, start + i + 1); // +1 for 1-based ranking
        if (!node) break;
        results[collected++] = node;
    }

    return collected;
}

int zset_range_by_score(ZSet* zset, double min_score, double max_score, ZNode** results, size_t max_results){
    if (!zset || !results || max_results == 0) return 0;

    // Find first node >= min_score
    ZNode* start_node = zset_lookge(zset, min_score, "", 0);
    if (!start_node) return 0;

    size_t collected = 0;
    ZNode* current = start_node;

    while (current && current->score <= max_score && collected < max_results) {
        results[collected++] = current;
        current = container_of(avl_next(&current->tree), ZNode, tree);
    }

    return collected;
}