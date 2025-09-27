#pragma once
#include <cstdint>
#include <cstddef>

// Intrusive AVL Tree Node structure
struct AVLNode {
    AVLNode* left = nullptr;
    AVLNode* right = nullptr;
    AVLNode* parent = nullptr;  // Parent pointer for easier traversal
    uint32_t height = 0;
    uint32_t cnt=0; //subtree's size
};

inline void avl_node_init(AVLNode* node){
    node->left = node->right = node->parent = nullptr;
    node->height = 1;
    node->cnt=1;
}

inline uint32_t avl_height(AVLNode* node){
    return node ? node->height : 0;
}

inline uint32_t avl_size(AVLNode* node){
    return node ? node->cnt : 0;
}

AVLNode* avl_insert(AVLNode* root, AVLNode* node, bool (*less)(AVLNode*, AVLNode*));
AVLNode* avl_del(AVLNode* node);
AVLNode* avl_offset(AVLNode* node, int64_t offset);
AVLNode* avl_next(AVLNode* node);
AVLNode* avl_prev(AVLNode* node);