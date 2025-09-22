#include <assert.h>
#include "../include/avltree.h"
#include <functional>

static uint32_t max(uint32_t a, uint32_t b){
    return a > b ? a : b;
}

static void avl_update(AVLNode* node){
    if(!node) return;
    node->height = max(avl_height(node->left), avl_height(node->right)) + 1;
    node->cnt = avl_size(node->left) + avl_size(node->right) + 1;
}

static int get_balance(AVLNode* node) {
    if (!node) return 0;
    return (int)avl_height(node->left) - (int)avl_height(node->right);
}

static AVLNode* rot_left(AVLNode* x){
    AVLNode* y = x->right;
    AVLNode* t2 = y->left;
    
    // 执行旋转
    y->left = x;
    x->right = t2;
    
    // 更新父指针
    y->parent = x->parent;
    x->parent = y;
    if (t2) t2->parent = x;
    
    // 更新高度和大小
    avl_update(x);
    avl_update(y);
    
    return y;
}

static AVLNode* rot_right(AVLNode* y){
    AVLNode* x = y->left;
    AVLNode* t2 = x->right;
    
    // 执行旋转
    x->right = y;
    y->left = t2;
    
    // 更新父指针
    x->parent = y->parent;
    y->parent = x;
    if (t2) t2->parent = y;
    
    // 更新高度和大小
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
        node->left->parent = node;
        return rot_right(node);
    }
    
    // 右左情况
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rot_right(node->right);
        node->right->parent = node;
        return rot_left(node);
    }
    
    return node;
}

// 从删除节点的路径向上修复树的平衡性
static AVLNode* fix_after_del(AVLNode* start_node) {
    if (!start_node) return nullptr;
    
    // 从start_node开始向上遍历到根节点
    AVLNode* root = start_node;
    
    // 找到根节点
    while(root->parent) {
        root = root->parent;
    }
    
    // 执行完整的树验证和修复，确保所有节点的父指针正确
    std::function<void(AVLNode*, AVLNode*)> validate_and_fix = [&validate_and_fix](AVLNode* node, AVLNode* parent) {
        if (!node) return;
        
        // 确保父指针正确
        node->parent = parent;
        
        // 递归修复左右子树
        validate_and_fix(node->left, node);
        validate_and_fix(node->right, node);
        
        // 更新当前节点的高度和大小
        avl_update(node);
    };
    
    // 从根节点开始验证和修复整棵树
    validate_and_fix(root, nullptr);
    
    // 现在执行平衡修复，确保树满足AVL性质
    std::function<AVLNode*(AVLNode*)> balance_tree = [&balance_tree](AVLNode* node) {
        if (!node) return (AVLNode*)nullptr;
        
        // 先平衡左右子树
        node->left = balance_tree(node->left);
        node->right = balance_tree(node->right);
        
        // 更新当前节点的高度和大小
        avl_update(node);
        
        // 执行平衡修复
        return avl_fix(node);
    };
    
    // 平衡整棵树
    root = balance_tree(root);
    
    // 最后再次验证父指针关系
    validate_and_fix(root, nullptr);
    
    return root; // 返回根节点
}

// 删除只有一个子节点或无子节点的节点
static AVLNode* avl_del_easy(AVLNode* node){ 
    assert(!node->left || !node->right);
    
    AVLNode* child = node->left ? node->left : node->right;
    AVLNode* parent = node->parent;
    
    // 保存要开始修复的节点
    AVLNode* start_fix_node = parent;
    
    // 连接子节点到父节点
    if(child){
        child->parent = parent;
    }
    
    if(parent){
        if(parent->left == node){
            parent->left = child;
        } else {
            parent->right = child;
        }
        
        // 从父节点开始向上修复
        return fix_after_del(parent);
    } else {
        // 如果删除的是根节点，子节点成为新的根节点
        return child;
    }
}

// 删除节点并返回新的根节点
AVLNode* avl_del(AVLNode* node){
    if(!node) return nullptr;
    
    // 情况1：节点最多只有一个子节点
    if(!node->left || !node->right){
        return avl_del_easy(node);
    }
    
    // 情况2：节点有两个子节点
    // 找到中序后继者（右子树中的最小节点）
    AVLNode* successor = node->right;
    while(successor->left){
        successor = successor->left;
    }
    
    // 保存节点信息
    AVLNode* node_parent = node->parent;
    AVLNode* succ_parent = successor->parent;
    AVLNode* succ_right = successor->right;
    
    // 1. 保存successor的值（在实际应用中会复制数据）
    // 在这个实现中，我们只需要重新连接指针
    
    // 2. 从树中移除successor
    if(succ_parent != node) {
        // 如果successor不是node的直接右子节点
        succ_parent->left = succ_right;
        if(succ_right) {
            succ_right->parent = succ_parent;
        }
        
        // 连接node的右子树到successor
        successor->right = node->right;
        if(successor->right) {
            successor->right->parent = successor;
        }
    }
    // 如果successor是node的直接右子节点，不需要特殊处理，保留其右子树
    // successor->right = nullptr; // 错误的代码，已移除
    
    // 3. 将successor移动到node的位置
    // 连接successor到node的左子树
    successor->left = node->left;
    if(successor->left) {
        successor->left->parent = successor;
    }
    
    // 连接successor到node的父节点
    successor->parent = node_parent;
    
    // 更新父节点的指针
    if(node_parent) {
        if(node_parent->left == node) {
            node_parent->left = successor;
        } else {
            node_parent->right = successor;
        }
    }
    
    // 4. 从successor的父节点开始向上修复树
    return fix_after_del(succ_parent);
}

// 按偏移量查找节点（基于0的索引）
AVLNode* avl_offset(AVLNode* node, int64_t offset){
    if(!node || offset < 0) return nullptr;
    
    // 在以node为根的子树中查找第 offset 个节点
    while(node){
        uint32_t left_size = avl_size(node->left);
        
        if(offset == left_size){
            // 找到了
            return node;
        } else if(offset < left_size){
            // 在左子树中
            node = node->left;
        } else {
            // 在右子树中
            offset = offset - left_size - 1;
            node = node->right;
        }
    }
    
    return nullptr; // 超出范围
}