#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <set>
#include <chrono>
#include <cassert>
#include <cstring>
#include "include/avltree.h"

struct TestNode {
    AVLNode tree;
    int value;

    TestNode(int val) : value(val) {
        avl_node_init(&tree);
    }
};

bool less_func(AVLNode* a, AVLNode* b) {
    TestNode* node_a = (TestNode*)((char*)a - offsetof(TestNode, tree));
    TestNode* node_b = (TestNode*)((char*)b - offsetof(TestNode, tree));
    return node_a->value < node_b->value;
}

TestNode* get_test_node(AVLNode* node) {
    return node ? (TestNode*)((char*)node - offsetof(TestNode, tree)) : nullptr;
}

class AVLTreeTester {
private:
    std::mt19937 rng;

    void print_tree_structure(AVLNode* root, int depth = 0) {
        if (!root) return;

        print_tree_structure(root->right, depth + 1);

        for (int i = 0; i < depth; i++) std::cout << "  ";
        TestNode* node = get_test_node(root);
        std::cout << node->value << "(h:" << root->height << ",s:" << root->cnt << ")" << std::endl;

        print_tree_structure(root->left, depth + 1);
    }

    void collect_inorder(AVLNode* root, std::vector<int>& result) {
        if (!root) return;
        collect_inorder(root->left, result);
        result.push_back(get_test_node(root)->value);
        collect_inorder(root->right, result);
    }

    bool verify_avl_properties(AVLNode* root) {
        if (!root) return true;

        // Check height property
        int left_height = avl_height(root->left);
        int right_height = avl_height(root->right);
        int expected_height = std::max(left_height, right_height) + 1;

        if (root->height != (uint32_t)expected_height) {
            std::cout << "Height property violated at node " << get_test_node(root)->value
                      << ": expected " << expected_height << ", got " << root->height << std::endl;
            return false;
        }

        // Check size property
        uint32_t left_size = avl_size(root->left);
        uint32_t right_size = avl_size(root->right);
        uint32_t expected_size = left_size + right_size + 1;

        if (root->cnt != expected_size) {
            std::cout << "Size property violated at node " << get_test_node(root)->value
                      << ": expected " << expected_size << ", got " << root->cnt << std::endl;
            return false;
        }

        // Check balance factor
        int balance = left_height - right_height;
        if (abs(balance) > 1) {
            std::cout << "Balance property violated at node " << get_test_node(root)->value
                      << ": balance factor is " << balance << std::endl;
            return false;
        }

        // Check parent pointers
        if (root->left && root->left->parent != root) {
            std::cout << "Parent pointer error: left child of " << get_test_node(root)->value
                      << " has wrong parent" << std::endl;
            return false;
        }

        if (root->right && root->right->parent != root) {
            std::cout << "Parent pointer error: right child of " << get_test_node(root)->value
                      << " has wrong parent" << std::endl;
            return false;
        }

        // Check BST property
        if (root->left) {
            TestNode* left_node = get_test_node(root->left);
            if (left_node->value >= get_test_node(root)->value) {
                std::cout << "BST property violated: left child " << left_node->value
                          << " >= parent " << get_test_node(root)->value << std::endl;
                return false;
            }
        }

        if (root->right) {
            TestNode* right_node = get_test_node(root->right);
            if (right_node->value <= get_test_node(root)->value) {
                std::cout << "BST property violated: right child " << right_node->value
                          << " <= parent " << get_test_node(root)->value << std::endl;
                return false;
            }
        }

        return verify_avl_properties(root->left) && verify_avl_properties(root->right);
    }

    void cleanup_nodes(std::vector<TestNode*>& nodes) {
        for (TestNode* node : nodes) {
            delete node;
        }
        nodes.clear();
    }

public:
    AVLTreeTester() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    bool test_basic_operations() {
        std::cout << "=== Testing Basic Operations ===" << std::endl;

        AVLNode* root = nullptr;
        std::vector<TestNode*> nodes;

        // Test insertion
        std::vector<int> values = {10, 5, 15, 3, 7, 12, 20, 1, 4, 6, 8, 11, 13, 18, 25};

        for (int val : values) {
            TestNode* node = new TestNode(val);
            nodes.push_back(node);
            root = avl_insert(root, &node->tree, less_func);

            if (!verify_avl_properties(root)) {
                std::cout << "❌ AVL properties violated after inserting " << val << std::endl;
                cleanup_nodes(nodes);
                return false;
            }
        }

        // Verify inorder traversal
        std::vector<int> result;
        collect_inorder(root, result);
        std::vector<int> expected = values;
        std::sort(expected.begin(), expected.end());

        if (result != expected) {
            std::cout << "❌ Inorder traversal incorrect" << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        // Test deletion
        std::vector<int> to_delete = {1, 15, 7, 20, 10};
        for (int val : to_delete) {
            TestNode* to_remove = nullptr;
            for (TestNode* node : nodes) {
                if (node->value == val) {
                    to_remove = node;
                    break;
                }
            }

            assert(to_remove != nullptr);
            root = avl_del(&to_remove->tree);

            // Remove from our tracking
            nodes.erase(std::find(nodes.begin(), nodes.end(), to_remove));
            delete to_remove;

            if (!verify_avl_properties(root)) {
                std::cout << "❌ AVL properties violated after deleting " << val << std::endl;
                cleanup_nodes(nodes);
                return false;
            }
        }

        cleanup_nodes(nodes);
        std::cout << "✅ Basic operations test passed" << std::endl;
        return true;
    }

    bool test_edge_cases() {
        std::cout << "=== Testing Edge Cases ===" << std::endl;

        // Test empty tree operations
        AVLNode* root = nullptr;
        root = avl_del(root);
        if (root != nullptr) {
            std::cout << "❌ Deleting from empty tree should return null" << std::endl;
            return false;
        }

        // Test single node
        TestNode* single_node = new TestNode(42);
        root = avl_insert(root, &single_node->tree, less_func);
        if (!verify_avl_properties(root)) {
            std::cout << "❌ Single node tree properties violated" << std::endl;
            delete single_node;
            return false;
        }

        root = avl_del(&single_node->tree);
        if (root != nullptr) {
            std::cout << "❌ Deleting single node should result in empty tree" << std::endl;
            delete single_node;
            return false;
        }
        delete single_node;

        // Test duplicate insertion
        TestNode* node1 = new TestNode(100);
        TestNode* node2 = new TestNode(100);

        root = avl_insert(root, &node1->tree, less_func);
        AVLNode* old_root = root;
        root = avl_insert(root, &node2->tree, less_func);

        if (root != old_root) {
            std::cout << "❌ Duplicate insertion should not change tree structure" << std::endl;
            delete node1;
            delete node2;
            return false;
        }

        delete node1;
        delete node2;

        std::cout << "✅ Edge cases test passed" << std::endl;
        return true;
    }

    bool test_navigation_functions() {
        std::cout << "=== Testing Navigation Functions ===" << std::endl;

        AVLNode* root = nullptr;
        std::vector<TestNode*> nodes;
        std::vector<int> values = {50, 25, 75, 12, 37, 62, 87, 6, 18, 31, 43};

        for (int val : values) {
            TestNode* node = new TestNode(val);
            nodes.push_back(node);
            root = avl_insert(root, &node->tree, less_func);
        }

        // Test avl_next and avl_prev
        std::vector<int> sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());

        // Find the minimum node
        AVLNode* current = root;
        while (current->left) {
            current = current->left;
        }

        // Traverse using avl_next
        std::vector<int> next_traversal;
        while (current) {
            next_traversal.push_back(get_test_node(current)->value);
            current = avl_next(current);
        }

        if (next_traversal != sorted_values) {
            std::cout << "❌ avl_next traversal incorrect" << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        // Find the maximum node
        current = root;
        while (current->right) {
            current = current->right;
        }

        // Traverse using avl_prev
        std::vector<int> prev_traversal;
        while (current) {
            prev_traversal.push_back(get_test_node(current)->value);
            current = avl_prev(current);
        }

        std::reverse(prev_traversal.begin(), prev_traversal.end());
        if (prev_traversal != sorted_values) {
            std::cout << "❌ avl_prev traversal incorrect" << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        // Test avl_offset
        TestNode* node_50 = nullptr;
        for (TestNode* node : nodes) {
            if (node->value == 50) {
                node_50 = node;
                break;
            }
        }
        assert(node_50 != nullptr);

        // Test positive offset
        AVLNode* offset_node = avl_offset(&node_50->tree, 3);
        if (!offset_node) {
            std::cout << "❌ avl_offset(50, +3) returned null" << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        // Test negative offset
        offset_node = avl_offset(&node_50->tree, -2);
        if (!offset_node) {
            std::cout << "❌ avl_offset(50, -2) returned null" << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        // Test zero offset
        offset_node = avl_offset(&node_50->tree, 0);
        if (offset_node != &node_50->tree) {
            std::cout << "❌ avl_offset(50, 0) should return same node" << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        cleanup_nodes(nodes);
        std::cout << "✅ Navigation functions test passed" << std::endl;
        return true;
    }

    bool test_stress_operations() {
        std::cout << "=== Testing Stress Operations ===" << std::endl;

        const int NUM_OPERATIONS = 1000;
        AVLNode* root = nullptr;
        std::vector<TestNode*> nodes;
        std::set<int> reference_set;

        std::uniform_int_distribution<int> value_dist(1, 1000);
        std::uniform_int_distribution<int> op_dist(0, 1); // 0: insert, 1: delete (remove search to simplify)

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int op = 0; op < NUM_OPERATIONS; op++) {
            int operation = op_dist(rng);
            int value = value_dist(rng);

            if (operation == 0 || nodes.empty()) { // Insert
                if (reference_set.find(value) == reference_set.end()) {
                    TestNode* node = new TestNode(value);
                    nodes.push_back(node);
                    root = avl_insert(root, &node->tree, less_func);
                    reference_set.insert(value);
                }
            } else if (operation == 1 && !nodes.empty()) { // Delete
                int idx = std::uniform_int_distribution<int>(0, nodes.size() - 1)(rng);
                TestNode* to_delete = nodes[idx];
                int val = to_delete->value;

                root = avl_del(&to_delete->tree);
                nodes.erase(nodes.begin() + idx);
                reference_set.erase(val);
                delete to_delete;
            }

            // Verify tree properties every 100 operations
            if (op % 100 == 99) {
                if (!verify_avl_properties(root)) {
                    std::cout << "❌ AVL properties violated at operation " << op + 1 << std::endl;
                    cleanup_nodes(nodes);
                    return false;
                }

                // Verify tree size matches reference
                if (avl_size(root) != reference_set.size()) {
                    std::cout << "❌ Tree size mismatch at operation " << op + 1
                              << ": expected " << reference_set.size()
                              << ", got " << avl_size(root) << std::endl;
                    cleanup_nodes(nodes);
                    return false;
                }

                std::cout << "✅ Operation " << op + 1 << " verified, tree size: " << avl_size(root) << std::endl;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "✅ Stress test completed in " << duration.count() << "ms" << std::endl;
        std::cout << "Final tree size: " << avl_size(root) << std::endl;

        cleanup_nodes(nodes);
        return true;
    }

    bool test_sequential_insertion() {
        std::cout << "=== Testing Sequential Insertion (Worst Case) ===" << std::endl;

        AVLNode* root = nullptr;
        std::vector<TestNode*> nodes;

        // Test ascending sequence
        for (int i = 1; i <= 1000; i++) {
            TestNode* node = new TestNode(i);
            nodes.push_back(node);
            root = avl_insert(root, &node->tree, less_func);

            if (!verify_avl_properties(root)) {
                std::cout << "❌ AVL properties violated during ascending insertion at " << i << std::endl;
                cleanup_nodes(nodes);
                return false;
            }
        }

        // Verify tree is balanced (height should be O(log n))
        int tree_height = avl_height(root);
        int expected_max_height = static_cast<int>(1.44 * log2(1000)) + 5; // AVL tree max height

        if (tree_height > expected_max_height) {
            std::cout << "❌ Tree too tall: height " << tree_height
                      << " > expected max " << expected_max_height << std::endl;
            cleanup_nodes(nodes);
            return false;
        }

        std::cout << "✅ Sequential insertion test passed, tree height: " << tree_height << std::endl;

        cleanup_nodes(nodes);
        return true;
    }

    bool run_all_tests() {
        std::cout << "🚀 Starting AVL Tree Production Test Suite" << std::endl;
        std::cout << "===========================================" << std::endl;

        bool all_passed = true;

        all_passed &= test_basic_operations();
        all_passed &= test_edge_cases();
        all_passed &= test_navigation_functions();
        all_passed &= test_sequential_insertion();
        all_passed &= test_stress_operations();

        std::cout << "===========================================" << std::endl;
        if (all_passed) {
            std::cout << "🎉 ALL AVL TREE TESTS PASSED! Production ready!" << std::endl;
        } else {
            std::cout << "💥 SOME TESTS FAILED! Fix required before production!" << std::endl;
        }

        return all_passed;
    }
};

int main() {
    AVLTreeTester tester;
    return tester.run_all_tests() ? 0 : 1;
}