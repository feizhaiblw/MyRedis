#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <map>
#include <chrono>
#include <cassert>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include "include/zset.h"
#include "include/hashtable.h"
#include "include/avltree.h"
#include "include/common.h"

class ZSetTester {
private:
    std::mt19937 rng;

    struct TestData {
        std::string name;
        double score;

        TestData(const std::string& n, double s) : name(n), score(s) {}

        bool operator<(const TestData& other) const {
            if (score != other.score) return score < other.score;
            return name < other.name;
        }

        bool operator==(const TestData& other) const {
            return score == other.score && name == other.name;
        }
    };

    void cleanup_zset(ZSet& zset) {
        zset_clear(&zset);
        memset(&zset, 0, sizeof(zset));
    }

    void collect_zset_inorder(AVLNode* root, std::vector<TestData>& result) {
        if (!root) return;

        collect_zset_inorder(root->left, result);

        ZNode* znode = container_of(root, ZNode, tree);
        std::string name(znode->name, znode->len);
        result.push_back(TestData(name, znode->score));

        collect_zset_inorder(root->right, result);
    }

    bool verify_zset_consistency(const ZSet& zset, const std::map<std::string, double>& reference) {
        // Verify hashtable size matches tree size
        if (avl_size(zset.root) != hm_size(const_cast<HMap*>(&zset.hmap))) {
            std::cout << "❌ Size mismatch: tree=" << avl_size(zset.root)
                      << ", hashtable=" << hm_size(const_cast<HMap*>(&zset.hmap)) << std::endl;
            return false;
        }

        if (avl_size(zset.root) != reference.size()) {
            std::cout << "❌ Size mismatch with reference: zset=" << avl_size(zset.root)
                      << ", reference=" << reference.size() << std::endl;
            return false;
        }

        // Verify every element in reference exists in zset
        for (const auto& pair : reference) {
            ZNode* node = zset_lookup(const_cast<ZSet*>(&zset), pair.first.c_str(), pair.first.size());
            if (!node) {
                std::cout << "❌ Missing element: " << pair.first << std::endl;
                return false;
            }
            if (node->score != pair.second) {
                std::cout << "❌ Score mismatch for " << pair.first
                          << ": expected=" << pair.second << ", got=" << node->score << std::endl;
                return false;
            }
        }

        // Verify tree ordering
        std::vector<TestData> tree_order;
        collect_zset_inorder(zset.root, tree_order);

        std::vector<TestData> expected_order;
        for (const auto& pair : reference) {
            expected_order.push_back(TestData(pair.first, pair.second));
        }
        std::sort(expected_order.begin(), expected_order.end());

        if (tree_order != expected_order) {
            std::cout << "❌ Tree ordering incorrect" << std::endl;
            return false;
        }

        return true;
    }

    std::string generate_random_string(int length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string result;
        result.reserve(length);

        std::uniform_int_distribution<int> dist(0, chars.size() - 1);
        for (int i = 0; i < length; ++i) {
            result += chars[dist(rng)];
        }
        return result;
    }

public:
    ZSetTester() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    bool test_basic_operations() {
        std::cout << "=== Testing Basic ZSet Operations ===" << std::endl;

        ZSet zset;
        memset(&zset, 0, sizeof(zset));
        std::map<std::string, double> reference;

        // Test basic insertion
        struct TestCase {
            std::string name;
            double score;
        } test_cases[] = {
            {"alice", 85.5},
            {"bob", 92.0},
            {"charlie", 88.5},
            {"diana", 95.0},
            {"eve", 87.0},
            {"frank", 91.5},
            {"grace", 89.0}
        };

        for (const auto& tc : test_cases) {
            bool inserted = zset_insert(&zset, tc.name.c_str(), tc.name.size(), tc.score);
            if (!inserted) {
                std::cout << "❌ Failed to insert " << tc.name << std::endl;
                cleanup_zset(zset);
                return false;
            }
            reference[tc.name] = tc.score;
        }

        if (!verify_zset_consistency(zset, reference)) {
            cleanup_zset(zset);
            return false;
        }

        // Test lookup
        for (const auto& tc : test_cases) {
            ZNode* node = zset_lookup(&zset, tc.name.c_str(), tc.name.size());
            if (!node || node->score != tc.score) {
                std::cout << "❌ Lookup failed for " << tc.name << std::endl;
                cleanup_zset(zset);
                return false;
            }
        }

        // Test score update
        bool updated = zset_insert(&zset, "alice", 5, 99.0);
        if (updated) {
            std::cout << "❌ Score update should return false" << std::endl;
            cleanup_zset(zset);
            return false;
        }
        reference["alice"] = 99.0;

        ZNode* alice = zset_lookup(&zset, "alice", 5);
        if (!alice || alice->score != 99.0) {
            std::cout << "❌ Score update failed" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        if (!verify_zset_consistency(zset, reference)) {
            cleanup_zset(zset);
            return false;
        }

        // Test deletion
        ZNode* bob = zset_lookup(&zset, "bob", 3);
        if (!bob) {
            std::cout << "❌ Bob not found before deletion" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        zset_delete(&zset, bob);
        reference.erase("bob");

        bob = zset_lookup(&zset, "bob", 3);
        if (bob) {
            std::cout << "❌ Bob still found after deletion" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        if (!verify_zset_consistency(zset, reference)) {
            cleanup_zset(zset);
            return false;
        }

        cleanup_zset(zset);
        std::cout << "✅ Basic operations test passed" << std::endl;
        return true;
    }

    bool test_range_queries() {
        std::cout << "=== Testing Range Query Operations ===" << std::endl;

        ZSet zset;
        memset(&zset, 0, sizeof(zset));
        std::map<std::string, double> reference;

        // Insert test data with various scores
        std::vector<std::pair<std::string, double>> test_data = {
            {"user1", 10.0}, {"user2", 20.0}, {"user3", 30.0}, {"user4", 40.0},
            {"user5", 50.0}, {"user6", 60.0}, {"user7", 70.0}, {"user8", 80.0},
            {"user9", 90.0}, {"user10", 100.0}
        };

        for (const auto& pair : test_data) {
            zset_insert(&zset, pair.first.c_str(), pair.first.size(), pair.second);
            reference[pair.first] = pair.second;
        }

        // Test zset_lookge with various scores
        struct RangeTest {
            double score;
            std::string name;
            size_t expected_results;
        } range_tests[] = {
            {25.0, "", 8},  // Should find user3 (30.0) and above
            {50.0, "", 6},  // Should find user5 (50.0) and above
            {75.0, "", 3},  // Should find user8 (80.0), user9 (90.0), user10 (100.0)
            {95.0, "", 1},  // Should find user10 (100.0)
            {101.0, "", 0}  // Should find nothing
        };

        for (const auto& test : range_tests) {
            ZNode* result = zset_lookge(&zset, test.score, test.name.c_str(), test.name.size());

            if (test.expected_results == 0) {
                if (result != nullptr) {
                    std::cout << "❌ Expected no results for score >= " << test.score << std::endl;
                    cleanup_zset(zset);
                    return false;
                }
            } else {
                if (!result) {
                    std::cout << "❌ Expected results for score >= " << test.score << std::endl;
                    cleanup_zset(zset);
                    return false;
                }

                // Verify the result is correct
                if (result->score < test.score) {
                    std::cout << "❌ Range query returned score " << result->score
                              << " which is less than " << test.score << std::endl;
                    cleanup_zset(zset);
                    return false;
                }

                // Count remaining elements
                size_t count = 0;
                ZNode* current_node = result;
                while (current_node) {
                    count++;
                    current_node = znode_offset(current_node, 1);
                }

                if (count != test.expected_results) {
                    std::cout << "❌ Range query count mismatch: expected "
                              << test.expected_results << ", got " << count << std::endl;
                    cleanup_zset(zset);
                    return false;
                }
            }
        }

        cleanup_zset(zset);
        std::cout << "✅ Range query test passed" << std::endl;
        return true;
    }

    bool test_offset_navigation() {
        std::cout << "=== Testing Offset Navigation ===" << std::endl;

        ZSet zset;
        memset(&zset, 0, sizeof(zset));

        // Insert ordered data
        std::vector<std::pair<std::string, double>> ordered_data;
        for (int i = 1; i <= 20; i++) {
            std::string name = "item" + std::to_string(i);
            double score = i * 10.0;
            ordered_data.push_back({name, score});
            zset_insert(&zset, name.c_str(), name.size(), score);
        }

        // Find middle element
        ZNode* middle = zset_lookup(&zset, "item10", 6);
        if (!middle) {
            std::cout << "❌ Could not find middle element" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Test forward navigation
        ZNode* current = middle;
        for (int i = 0; i < 5; i++) {
            if (!current) {
                std::cout << "❌ Unexpected null during forward navigation at step " << i << std::endl;
                cleanup_zset(zset);
                return false;
            }
            current = znode_offset(current, 1);
        }

        if (!current || current->score != 150.0) {  // item15 has score 150
            std::cout << "❌ Forward navigation incorrect" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Test backward navigation
        current = middle;
        for (int i = 0; i < 3; i++) {
            if (!current) {
                std::cout << "❌ Unexpected null during backward navigation at step " << i << std::endl;
                cleanup_zset(zset);
                return false;
            }
            current = znode_offset(current, -1);
        }

        if (!current || current->score != 70.0) {  // item7 has score 70
            std::cout << "❌ Backward navigation incorrect" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Test zero offset
        current = znode_offset(middle, 0);
        if (current != middle) {
            std::cout << "❌ Zero offset should return same node" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Test boundary conditions
        ZNode* first = zset_lookup(&zset, "item1", 5);
        ZNode* before_first = znode_offset(first, -1);
        if (before_first != nullptr) {
            std::cout << "❌ Offset before first element should return null" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        ZNode* last = zset_lookup(&zset, "item20", 6);
        ZNode* after_last = znode_offset(last, 1);
        if (after_last != nullptr) {
            std::cout << "❌ Offset after last element should return null" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        cleanup_zset(zset);
        std::cout << "✅ Offset navigation test passed" << std::endl;
        return true;
    }

    bool test_duplicate_handling() {
        std::cout << "=== Testing Duplicate Handling ===" << std::endl;

        ZSet zset;
        memset(&zset, 0, sizeof(zset));

        // Insert initial element
        bool inserted = zset_insert(&zset, "test", 4, 50.0);
        if (!inserted) {
            std::cout << "❌ Initial insertion failed" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Try to insert duplicate - should update
        bool updated = zset_insert(&zset, "test", 4, 75.0);
        if (updated) {
            std::cout << "❌ Duplicate insertion should return false (update)" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Verify update worked
        ZNode* node = zset_lookup(&zset, "test", 4);
        if (!node || node->score != 75.0) {
            std::cout << "❌ Score update failed" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Verify tree still has only one element
        if (avl_size(zset.root) != 1) {
            std::cout << "❌ Tree should have only one element after update" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        cleanup_zset(zset);
        std::cout << "✅ Duplicate handling test passed" << std::endl;
        return true;
    }

    bool test_stress_operations() {
        std::cout << "=== Testing Stress Operations ===" << std::endl;

        const int NUM_OPERATIONS = 5000;
        ZSet zset;
        memset(&zset, 0, sizeof(zset));
        std::map<std::string, double> reference;

        std::uniform_real_distribution<double> score_dist(0.0, 1000.0);
        std::uniform_int_distribution<int> name_len_dist(3, 10);
        std::uniform_int_distribution<int> op_dist(0, 3); // 0: insert, 1: update, 2: delete, 3: lookup

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int op = 0; op < NUM_OPERATIONS; op++) {
            int operation = op_dist(rng);

            if (operation == 0 || reference.empty()) { // Insert
                std::string name = generate_random_string(name_len_dist(rng));
                double score = score_dist(rng);

                bool should_be_new = (reference.find(name) == reference.end());
                bool result = zset_insert(&zset, name.c_str(), name.size(), score);

                if (result != should_be_new) {
                    std::cout << "❌ Insert result mismatch for " << name
                              << " at operation " << op << std::endl;
                    cleanup_zset(zset);
                    return false;
                }

                reference[name] = score;

            } else if (operation == 1 && !reference.empty()) { // Update
                auto it = reference.begin();
                std::advance(it, std::uniform_int_distribution<int>(0, reference.size() - 1)(rng));

                double new_score = score_dist(rng);
                bool result = zset_insert(&zset, it->first.c_str(), it->first.size(), new_score);

                if (result) {
                    std::cout << "❌ Update should return false for " << it->first
                              << " at operation " << op << std::endl;
                    cleanup_zset(zset);
                    return false;
                }

                reference[it->first] = new_score;

            } else if (operation == 2 && !reference.empty()) { // Delete
                auto it = reference.begin();
                std::advance(it, std::uniform_int_distribution<int>(0, reference.size() - 1)(rng));

                ZNode* node = zset_lookup(&zset, it->first.c_str(), it->first.size());
                if (!node) {
                    std::cout << "❌ Node not found for deletion: " << it->first
                              << " at operation " << op << std::endl;
                    cleanup_zset(zset);
                    return false;
                }

                zset_delete(&zset, node);
                reference.erase(it);

            } else if (operation == 3 && !reference.empty()) { // Lookup
                auto it = reference.begin();
                std::advance(it, std::uniform_int_distribution<int>(0, reference.size() - 1)(rng));

                ZNode* node = zset_lookup(&zset, it->first.c_str(), it->first.size());
                if (!node || node->score != it->second) {
                    std::cout << "❌ Lookup failed for " << it->first
                              << " at operation " << op << std::endl;
                    cleanup_zset(zset);
                    return false;
                }
            }

            // Verify consistency every 500 operations
            if (op % 500 == 499) {
                if (!verify_zset_consistency(zset, reference)) {
                    std::cout << "❌ Consistency check failed at operation " << op + 1 << std::endl;
                    cleanup_zset(zset);
                    return false;
                }
                std::cout << "✅ Operation " << op + 1 << " verified, zset size: " << avl_size(zset.root) << std::endl;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "✅ Stress test completed in " << duration.count() << "ms" << std::endl;
        std::cout << "Final zset size: " << avl_size(zset.root) << std::endl;

        cleanup_zset(zset);
        return true;
    }

    bool test_memory_and_cleanup() {
        std::cout << "=== Testing Memory Management ===" << std::endl;

        ZSet zset;
        memset(&zset, 0, sizeof(zset));

        // Insert many elements
        const int NUM_ELEMENTS = 1000;
        for (int i = 0; i < NUM_ELEMENTS; i++) {
            std::string name = "item" + std::to_string(i);
            zset_insert(&zset, name.c_str(), name.size(), i * 1.0);
        }

        if (avl_size(zset.root) != NUM_ELEMENTS) {
            std::cout << "❌ Size mismatch after bulk insert" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        // Clear the entire set
        zset_clear(&zset);

        if (zset.root != nullptr || hm_size(&zset.hmap) != 0) {
            std::cout << "❌ ZSet not properly cleared" << std::endl;
            return false;
        }

        // Verify we can reuse the zset
        zset_insert(&zset, "new_item", 8, 42.0);
        if (avl_size(zset.root) != 1) {
            std::cout << "❌ Cannot reuse cleared zset" << std::endl;
            cleanup_zset(zset);
            return false;
        }

        cleanup_zset(zset);
        std::cout << "✅ Memory management test passed" << std::endl;
        return true;
    }

    bool run_all_tests() {
        std::cout << "🚀 Starting ZSet Production Test Suite" << std::endl;
        std::cout << "======================================" << std::endl;

        bool all_passed = true;

        all_passed &= test_basic_operations();
        all_passed &= test_range_queries();
        all_passed &= test_offset_navigation();
        all_passed &= test_duplicate_handling();
        all_passed &= test_memory_and_cleanup();
        all_passed &= test_stress_operations();

        std::cout << "======================================" << std::endl;
        if (all_passed) {
            std::cout << "🎉 ALL ZSET TESTS PASSED! Production ready!" << std::endl;
        } else {
            std::cout << "💥 SOME TESTS FAILED! Fix required before production!" << std::endl;
        }

        return all_passed;
    }
};

int main() {
    ZSetTester tester;
    return tester.run_all_tests() ? 0 : 1;
}