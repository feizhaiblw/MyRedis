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
        return score>znode1->score;
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
    AVLNode* parent=nullptr;
    AVLNode** from=&zset->root;
    while(*from){
        parent=*from;
        from=zless(&node->tree,parent)?&parent->left:&parent->right;
    }
    *from=&node->tree;
    node->tree.parent=parent;
    zset->root=avl_fix(&node->tree);
}

//update the score of node
static void zset_update(ZSet* zset,ZNode* node,double score){
    if(node->score==score){
        return;
    }
    zset->root=avl_del(&node->tree);
    avl_node_init(&node->tree);
    node->score=score;
    tree_insert(zset,node);
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
    HKey hkey;
    hkey.len=node->len;
    hkey.name=node->name;
    hkey.node.hcode=node->hmap.hcode;
    HNode* found=hm_delete(&zset->hmap,&hkey.node,&hcmp);
    assert(found);
    zset->root=avl_del(&node->tree);
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
    hm_clear(&zset->hmap);
    tree_dispose(zset->root);
    zset->root=nullptr;
}