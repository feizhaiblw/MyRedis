#include <assert.h>
#include "../include/avltree.h"
#include <functional>

static uint32_t max(uint32_t a, uint32_t b){
    return a > b ? a : b;
}

void avl_update(AVLNode* node){
    if(!node) return;
    node->height = max(avl_height(node->left), avl_height(node->right)) + 1;
    node->cnt = avl_size(node->left) + avl_size(node->right) + 1;
}

static int get_balance(AVLNode* node) {
    if (!node) return 0;
    return (int)avl_height(node->left) - (int)avl_height(node->right);
}

static AVLNode* rot_left(AVLNode* x){
    assert(x && x->right);
    AVLNode* y = x->right;
    AVLNode* t2 = y->left;
    AVLNode* parent = x->parent;

    y->left = x;
    x->right = t2;

    if (t2) t2->parent = x;
    x->parent = y;
    y->parent = parent;

    if (parent) {
        if (parent->left == x) {
            parent->left = y;
        } else if (parent->right == x) {
            parent->right = y;
        }
    }

    avl_update(x);
    avl_update(y);
    return y;
}

static AVLNode* rot_right(AVLNode* y){
    assert(y && y->left);
    AVLNode* x = y->left;
    AVLNode* t2 = x->right;
    AVLNode* parent = y->parent;

    x->right = y;
    y->left = t2;

    if (t2) t2->parent = y;
    y->parent = x;
    x->parent = parent;

    if (parent) {
        if (parent->left == y) {
            parent->left = x;
        } else if (parent->right == y) {
            parent->right = x;
        }
    }

    avl_update(y);
    avl_update(x);
    return x;
}

// 简单的AVL修复函数
AVLNode* avl_fix(AVLNode* node){
    if(!node) return nullptr;
    
    avl_update(node);
    
    int balance = get_balance(node);
    
    // 左左情况
    if (balance > 1 && get_balance(node->left) >= 0) {
        return rot_right(node);
    }
    
    // 右右情况
    if (balance < -1 && get_balance(node->right) <= 0) {
        return rot_left(node);
    }
    
    // 左右情况
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = rot_left(node->left);
        if (node->left) node->left->parent = node;
        return rot_right(node);
    }

    // 右左情况
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rot_right(node->right);
        if (node->right) node->right->parent = node;
        return rot_left(node);
    }
    
    return node;
}

// Insert helper function that returns the new root after insertion
static AVLNode* avl_insert_helper(AVLNode* root, AVLNode* node, bool (*less)(AVLNode*, AVLNode*)) {
    // Base case: empty tree
    if (!root) {
        avl_node_init(node);
        return node;
    }

    // Recursive insertion
    if (less(node, root)) {
        root->left = avl_insert_helper(root->left, node, less);
        root->left->parent = root;
    } else if (less(root, node)) {
        root->right = avl_insert_helper(root->right, node, less);
        root->right->parent = root;
    } else {
        // Equal nodes - do not insert duplicates
        return root;
    }

    // Update and rebalance
    avl_update(root);
    return avl_fix(root);
}

// Insert a node into AVL tree and return the new root
AVLNode* avl_insert(AVLNode* root, AVLNode* node, bool (*less)(AVLNode*, AVLNode*)) {
    return avl_insert_helper(root, node, less);
}

// Find minimum node in subtree
static AVLNode* avl_find_min(AVLNode* node) {
    if (!node) return nullptr;
    while (node->left) {
        node = node->left;
    }
    return node;
}

// Simplified AVL deletion function
AVLNode* avl_del(AVLNode* node) {
    if (!node) return nullptr;

    // Find root of the tree
    AVLNode* root = node;
    while (root->parent) {
        root = root->parent;
    }

    // Case 1: Node has at most one child
    if (!node->left || !node->right) {
        AVLNode* child = node->left ? node->left : node->right;
        AVLNode* parent = node->parent;

        // Connect child to parent
        if (child) child->parent = parent;

        if (parent) {
            if (parent->left == node) {
                parent->left = child;
            } else {
                parent->right = child;
            }

            // Rebalance from parent upward
            AVLNode* current = parent;
            while (current) {
                avl_update(current);
                current = avl_fix(current);
                if (!current->parent) return current;
                current = current->parent;
            }
            return root; // Should not reach here
        } else {
            // Node was root
            return child;
        }
    }

    // Case 2: Node has both children
    // Find inorder successor (leftmost node in right subtree)
    AVLNode* successor = avl_find_min(node->right);
    AVLNode* succ_parent = successor->parent;
    AVLNode* succ_right = successor->right;

    // Remove successor from its current position
    if (succ_parent->left == successor) {
        succ_parent->left = succ_right;
    } else {
        succ_parent->right = succ_right;
    }
    if (succ_right) succ_right->parent = succ_parent;

    // Replace node with successor
    successor->left = node->left;
    successor->right = node->right;
    successor->parent = node->parent;

    if (node->left) node->left->parent = successor;
    if (node->right) node->right->parent = successor;

    if (node->parent) {
        if (node->parent->left == node) {
            node->parent->left = successor;
        } else {
            node->parent->right = successor;
        }
    } else {
        // Node was root
        root = successor;
    }

    // Rebalance from where successor was removed
    AVLNode* start_point = (succ_parent == node) ? successor : succ_parent;
    AVLNode* current = start_point;

    while (current) {
        avl_update(current);
        current = avl_fix(current);
        if (!current->parent) return current;
        current = current->parent;
    }

    return root;
}

// 从指定节点开始的相对偏移查找
AVLNode* avl_offset(AVLNode* node, int64_t offset){
    if(!node) return nullptr;

    if(offset == 0) return node;

    if(offset > 0) {
        // 正向偏移：寻找中序遍历中的后续节点
        for(int64_t i = 0; i < offset; i++) {
            node = avl_next(node);
            if(!node) return nullptr;
        }
        return node;
    } else {
        // 负向偏移：寻找中序遍历中的前驱节点
        for(int64_t i = 0; i < -offset; i++) {
            node = avl_prev(node);
            if(!node) return nullptr;
        }
        return node;
    }
}

// 中序遍历的下一个节点
AVLNode* avl_next(AVLNode* node) {
    if (!node) return nullptr;

    if (node->right) {
        // 如果有右子树，找右子树的最左节点
        node = node->right;
        while (node->left) {
            node = node->left;
        }
        return node;
    } else {
        // 如果没有右子树，向上找第一个使当前节点在其左子树中的祖先
        AVLNode* parent = node->parent;
        while (parent && node == parent->right) {
            node = parent;
            parent = parent->parent;
        }
        return parent;
    }
}

// 中序遍历的前一个节点
AVLNode* avl_prev(AVLNode* node) {
    if (!node) return nullptr;

    if (node->left) {
        // 如果有左子树，找左子树的最右节点
        node = node->left;
        while (node->right) {
            node = node->right;
        }
        return node;
    } else {
        // 如果没有左子树，向上找第一个使当前节点在其右子树中的祖先
        AVLNode* parent = node->parent;
        while (parent && node == parent->left) {
            node = parent;
            parent = parent->parent;
        }
        return parent;
    }
}
