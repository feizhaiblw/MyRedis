#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <random>
#include "include/avltree.h"

struct TestNode {
    AVLNode tree;
    int value;
    
    TestNode(int val) : value(val) {
        avl_node_init(&tree);
    }
};

// 从AVLNode获取TestNode
TestNode* get_test_node(AVLNode* node) {
    if (!node) return nullptr;
    return (TestNode*)((char*)node - offsetof(TestNode, tree));
}

// 打印节点详细信息
void print_node_details(AVLNode* node) {
    if (!node) {
        std::cout << "null" << std::endl;
        return;
    }
    
    TestNode* test_node = get_test_node(node);
    std::cout << "节点值: " << test_node->value 
              << ", 高度: " << node->height 
              << ", 大小: " << node->cnt
              << ", 左子节点: " << (node->left ? std::to_string(get_test_node(node->left)->value) : "null")
              << ", 右子节点: " << (node->right ? std::to_string(get_test_node(node->right)->value) : "null")
              << ", 父节点: " << (node->parent ? std::to_string(get_test_node(node->parent)->value) : "null")
              << std::endl;
}

// 中序遍历打印树结构
void inorder_print(AVLNode* node) {
    if (!node) return;
    
    inorder_print(node->left);
    print_node_details(node);
    inorder_print(node->right);
}

// 验证AVL树性质并打印详细信息
bool check_avl(AVLNode* node) {
    if (!node) return true;
    
    // 检查高度
    uint32_t lh = avl_height(node->left);
    uint32_t rh = avl_height(node->right);
    uint32_t expected_height = std::max(lh, rh) + 1;
    
    if (node->height != expected_height) {
        std::cout << "高度错误 at " << get_test_node(node)->value 
                  << ", 实际高度=" << node->height 
                  << ", 期望高度=" << expected_height 
                  << std::endl;
        return false;
    }
    
    // 检查大小
    uint32_t ls = avl_size(node->left);
    uint32_t rs = avl_size(node->right);
    uint32_t expected_size = ls + rs + 1;
    
    if (node->cnt != expected_size) {
        std::cout << "大小错误 at " << get_test_node(node)->value 
                  << ", 实际大小=" << node->cnt 
                  << ", 期望大小=" << expected_size 
                  << std::endl;
        return false;
    }
    
    // 检查平衡因子
    int balance = (int)lh - (int)rh;
    if (balance < -1 || balance > 1) {
        std::cout << "平衡因子错误 at " << get_test_node(node)->value 
                  << ": " << balance 
                  << std::endl;
        return false;
    }
    
    // 检查父指针
    if (node->left && node->left->parent != node) {
        std::cout << "左子节点父指针错误 at " << get_test_node(node)->value 
                  << "->" << get_test_node(node->left)->value 
                  << std::endl;
        return false;
    }
    
    if (node->right && node->right->parent != node) {
        std::cout << "右子节点父指针错误 at " << get_test_node(node)->value 
                  << "->" << get_test_node(node->right)->value 
                  << std::endl;
        return false;
    }
    
    // 递归检查子树
    return check_avl(node->left) && check_avl(node->right);
}

// 构建一个小型AVL树并手动设置高度和大小
AVLNode* build_small_tree(std::vector<TestNode*>& nodes) {
    // 创建节点
    TestNode* node10 = new TestNode(10);
    TestNode* node5 = new TestNode(5);
    TestNode* node15 = new TestNode(15);
    TestNode* node3 = new TestNode(3);
    TestNode* node7 = new TestNode(7);
    TestNode* node12 = new TestNode(12);
    TestNode* node18 = new TestNode(18);
    
    // 保存到vector以便清理
    nodes = {node10, node5, node15, node3, node7, node12, node18};
    
    // 构建树结构
    node10->tree.left = &node5->tree;
    node10->tree.right = &node15->tree;
    node5->tree.parent = &node10->tree;
    node15->tree.parent = &node10->tree;
    
    node5->tree.left = &node3->tree;
    node5->tree.right = &node7->tree;
    node3->tree.parent = &node5->tree;
    node7->tree.parent = &node5->tree;
    
    node15->tree.left = &node12->tree;
    node15->tree.right = &node18->tree;
    node12->tree.parent = &node15->tree;
    node18->tree.parent = &node15->tree;
    
    // 手动更新高度和大小
    node3->tree.height = 1; node3->tree.cnt = 1;
    node7->tree.height = 1; node7->tree.cnt = 1;
    node12->tree.height = 1; node12->tree.cnt = 1;
    node18->tree.height = 1; node18->tree.cnt = 1;
    node5->tree.height = 2; node5->tree.cnt = 3;
    node15->tree.height = 2; node15->tree.cnt = 3;
    node10->tree.height = 3; node10->tree.cnt = 7;
    
    return &node10->tree;
}

// 测试删除叶子节点
void test_delete_leaf() {
    std::cout << "\n===== 测试：删除叶子节点 =====" << std::endl;
    
    std::vector<TestNode*> nodes;
    AVLNode* root = build_small_tree(nodes);
    
    std::cout << "删除前的树结构：" << std::endl;
    inorder_print(root);
    
    std::cout << "\n删除叶子节点 (值=3)..." << std::endl;
    // 找到值为3的节点
    TestNode* node_to_delete = nullptr;
    for (auto node : nodes) {
        if (node->value == 3) {
            node_to_delete = node;
            break;
        }
    }
    
    AVLNode* new_root = avl_del(&node_to_delete->tree);
    
    std::cout << "\n删除后的树结构：" << std::endl;
    inorder_print(new_root);
    
    std::cout << "\n删除后的AVL验证：" << (check_avl(new_root) ? "通过" : "失败") << std::endl;
    
    // 清理内存
    for (auto node : nodes) {
        delete node;
    }
}

// 测试删除只有一个子节点的节点
void test_delete_one_child() {
    std::cout << "\n===== 测试：删除只有一个子节点的节点 =====" << std::endl;
    
    // 创建一个特殊的树，其中node5只有一个子节点
    std::vector<TestNode*> nodes;
    TestNode* node10 = new TestNode(10);
    TestNode* node5 = new TestNode(5);
    TestNode* node15 = new TestNode(15);
    TestNode* node3 = new TestNode(3);
    TestNode* node12 = new TestNode(12);
    TestNode* node18 = new TestNode(18);
    
    nodes = {node10, node5, node15, node3, node12, node18};
    
    // 构建树结构
    node10->tree.left = &node5->tree;
    node10->tree.right = &node15->tree;
    node5->tree.parent = &node10->tree;
    node15->tree.parent = &node10->tree;
    
    node5->tree.left = &node3->tree;
    node5->tree.right = nullptr;  // 只有左子节点
    node3->tree.parent = &node5->tree;
    
    node15->tree.left = &node12->tree;
    node15->tree.right = &node18->tree;
    node12->tree.parent = &node15->tree;
    node18->tree.parent = &node15->tree;
    
    // 手动更新高度和大小
    node3->tree.height = 1; node3->tree.cnt = 1;
    node12->tree.height = 1; node12->tree.cnt = 1;
    node18->tree.height = 1; node18->tree.cnt = 1;
    node5->tree.height = 2; node5->tree.cnt = 2;
    node15->tree.height = 2; node15->tree.cnt = 3;
    node10->tree.height = 3; node10->tree.cnt = 6;
    
    AVLNode* root = &node10->tree;
    
    std::cout << "删除前的树结构：" << std::endl;
    inorder_print(root);
    
    std::cout << "\n删除只有一个子节点的节点 (值=5)..." << std::endl;
    AVLNode* new_root = avl_del(&node5->tree);
    
    std::cout << "\n删除后的树结构：" << std::endl;
    inorder_print(new_root);
    
    std::cout << "\n删除后的AVL验证：" << (check_avl(new_root) ? "通过" : "失败") << std::endl;
    
    // 清理内存
    for (auto node : nodes) {
        delete node;
    }
}

// 测试删除有两个子节点的节点
void test_delete_two_children() {
    std::cout << "\n===== 测试：删除有两个子节点的节点 =====" << std::endl;
    
    std::vector<TestNode*> nodes;
    AVLNode* root = build_small_tree(nodes);
    
    std::cout << "删除前的树结构：" << std::endl;
    inorder_print(root);
    
    std::cout << "\n删除有两个子节点的根节点 (值=10)..." << std::endl;
    AVLNode* new_root = avl_del(root);
    
    std::cout << "\n删除后的树结构：" << std::endl;
    inorder_print(new_root);
    
    std::cout << "\n删除后的AVL验证：" << (check_avl(new_root) ? "通过" : "失败") << std::endl;
    
    // 清理内存
    for (auto node : nodes) {
        delete node;
    }
}

// 测试avl_offset函数
void test_avl_offset() {
    std::cout << "\n===== 测试：avl_offset函数 =====" << std::endl;
    
    std::vector<TestNode*> nodes;
    AVLNode* root = build_small_tree(nodes);
    
    std::cout << "树的中序遍历：" << std::endl;
    inorder_print(root);
    
    // 测试不同的偏移量
    std::vector<int64_t> offsets = {0, 2, 3, 6, 7}; // 7是超出范围的索引
    for (int64_t offset : offsets) {
        AVLNode* node = avl_offset(root, offset);
        if (node) {
            std::cout << "偏移量 " << offset << " 对应的节点值: " << get_test_node(node)->value << std::endl;
        } else {
            std::cout << "偏移量 " << offset << " 超出范围" << std::endl;
        }
    }
    
    // 清理内存
    for (auto node : nodes) {
        delete node;
    }
}

// 测试边界情况：空树和单节点树
void test_edge_cases() {
    std::cout << "\n===== 测试：边界情况 =====" << std::endl;
    
    // 测试空树
    std::cout << "\n测试空树删除..." << std::endl;
    AVLNode* null_result = avl_del(nullptr);
    std::cout << "空树删除结果：" << (null_result == nullptr ? "正确 (nullptr)" : "错误") << std::endl;
    
    // 测试单节点树
    std::cout << "\n测试单节点树删除..." << std::endl;
    TestNode* single_node = new TestNode(42);
    AVLNode* single_root = &single_node->tree;
    
    std::cout << "删除前的树结构：" << std::endl;
    inorder_print(single_root);
    
    AVLNode* new_single_root = avl_del(single_root);
    std::cout << "删除后的树结构：" << (new_single_root == nullptr ? "空树" : "非空树") << std::endl;
    
    // 清理内存
    delete single_node;
}

// 测试旋转情况
void test_rotations() {
    std::cout << "\n===== 测试：旋转情况 =====" << std::endl;
    
    // 创建一个需要左旋的情况（右右）
    std::cout << "\n测试右旋情况（左左）..." << std::endl;
    std::vector<TestNode*> left_left_nodes;
    TestNode* ll_node30 = new TestNode(30);
    TestNode* ll_node20 = new TestNode(20);
    TestNode* ll_node10 = new TestNode(10);
    
    left_left_nodes = {ll_node30, ll_node20, ll_node10};
    
    ll_node30->tree.left = &ll_node20->tree;
    ll_node30->tree.right = nullptr;
    ll_node20->tree.parent = &ll_node30->tree;
    ll_node20->tree.left = &ll_node10->tree;
    ll_node20->tree.right = nullptr;
    ll_node10->tree.parent = &ll_node20->tree;
    
    ll_node10->tree.height = 1; ll_node10->tree.cnt = 1;
    ll_node20->tree.height = 2; ll_node20->tree.cnt = 2;
    ll_node30->tree.height = 3; ll_node30->tree.cnt = 3;
    
    AVLNode* ll_root = &ll_node30->tree;
    std::cout << "修复前的树结构：" << std::endl;
    inorder_print(ll_root);
    
    AVLNode* ll_fixed = avl_fix(ll_root);
    std::cout << "修复后的树结构：" << std::endl;
    inorder_print(ll_fixed);
    std::cout << "修复后的AVL验证：" << (check_avl(ll_fixed) ? "通过" : "失败") << std::endl;
    
    // 清理内存
    for (auto node : left_left_nodes) {
        delete node;
    }
    
    // 创建一个需要右旋的情况（左左）
    std::cout << "\n测试左旋情况（右右）..." << std::endl;
    std::vector<TestNode*> right_right_nodes;
    TestNode* rr_node10 = new TestNode(10);
    TestNode* rr_node20 = new TestNode(20);
    TestNode* rr_node30 = new TestNode(30);
    
    right_right_nodes = {rr_node10, rr_node20, rr_node30};
    
    rr_node10->tree.left = nullptr;
    rr_node10->tree.right = &rr_node20->tree;
    rr_node20->tree.parent = &rr_node10->tree;
    rr_node20->tree.left = nullptr;
    rr_node20->tree.right = &rr_node30->tree;
    rr_node30->tree.parent = &rr_node20->tree;
    
    rr_node30->tree.height = 1; rr_node30->tree.cnt = 1;
    rr_node20->tree.height = 2; rr_node20->tree.cnt = 2;
    rr_node10->tree.height = 3; rr_node10->tree.cnt = 3;
    
    AVLNode* rr_root = &rr_node10->tree;
    std::cout << "修复前的树结构：" << std::endl;
    inorder_print(rr_root);
    
    AVLNode* rr_fixed = avl_fix(rr_root);
    std::cout << "修复后的树结构：" << std::endl;
    inorder_print(rr_fixed);
    std::cout << "修复后的AVL验证：" << (check_avl(rr_fixed) ? "通过" : "失败") << std::endl;
    
    // 清理内存
    for (auto node : right_right_nodes) {
        delete node;
    }
}

// 测试随机删除操作
void test_random_deletes() {
    std::cout << "\n===== 测试：随机删除操作 =====" << std::endl;
    
    // 创建一个较大的树
    const int TREE_SIZE = 20;
    std::vector<TestNode*> random_nodes;
    
    // 创建节点
    for (int i = 0; i < TREE_SIZE; i++) {
        random_nodes.push_back(new TestNode(i * 10));
    }
    
    // 手动构建一个相对平衡的树
    // 这个例子简化了构建过程，实际应用中应该有专门的插入函数
    // 这里我们构建一个完全二叉树的结构
    for (int i = 0; i < TREE_SIZE; i++) {
        int left_idx = 2 * i + 1;
        int right_idx = 2 * i + 2;
        
        if (left_idx < TREE_SIZE) {
            random_nodes[i]->tree.left = &random_nodes[left_idx]->tree;
            random_nodes[left_idx]->tree.parent = &random_nodes[i]->tree;
        }
        
        if (right_idx < TREE_SIZE) {
            random_nodes[i]->tree.right = &random_nodes[right_idx]->tree;
            random_nodes[right_idx]->tree.parent = &random_nodes[i]->tree;
        }
    }
    
    // 手动更新高度和大小（这里简化处理）
    for (auto node : random_nodes) {
        node->tree.height = 1;
        node->tree.cnt = 1;
    }
    
    AVLNode* random_root = &random_nodes[0]->tree;
    
    // 打乱节点顺序进行随机删除
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(random_nodes.begin(), random_nodes.end(), g);
    
    // 随机删除一半的节点
    std::cout << "开始随机删除节点..." << std::endl;
    int nodes_to_delete = TREE_SIZE / 2;
    for (int i = 0; i < nodes_to_delete; i++) {
        TestNode* node_to_delete = random_nodes[i];
        std::cout << "删除节点值: " << node_to_delete->value << std::endl;
        random_root = avl_del(&node_to_delete->tree);
        
        if (!check_avl(random_root)) {
            std::cout << "AVL验证失败，提前结束随机删除测试" << std::endl;
            break;
        }
    }
    
    std::cout << "\n随机删除后的树结构：" << std::endl;
    inorder_print(random_root);
    
    std::cout << "\n随机删除后的AVL验证：" << (check_avl(random_root) ? "通过" : "失败") << std::endl;
    
    // 清理内存
    for (auto node : random_nodes) {
        delete node;
    }
}

int main() {
    std::cout << "===== AVL树完整测试套件 =====" << std::endl;
    
    // 运行所有测试
    test_edge_cases();           // 边界情况测试
    test_delete_leaf();          // 删除叶子节点测试
    test_delete_one_child();     // 删除只有一个子节点的节点测试
    test_delete_two_children();  // 删除有两个子节点的节点测试
    test_avl_offset();           // avl_offset函数测试
    test_rotations();            // 旋转情况测试
    test_random_deletes();       // 随机删除操作测试
    
    std::cout << "\n===== 所有测试完成 =====" << std::endl;
    return 0;
}