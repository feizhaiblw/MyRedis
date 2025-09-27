#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include "include/zset.h"
#include "include/common.h"

// Test helper functions
void test_zset_rank() {
    std::cout << "Testing zset_rank..." << std::endl;

    ZSet zset = {};

    // Add some test data
    assert(zset_insert(&zset, "member1", 7, 1.0));
    assert(zset_insert(&zset, "member2", 7, 2.0));
    assert(zset_insert(&zset, "member3", 7, 3.0));
    assert(zset_insert(&zset, "member4", 7, 4.0));

    // Test rankings
    assert(zset_rank(&zset, "member1", 7) == 1); // First element
    assert(zset_rank(&zset, "member2", 7) == 2); // Second element
    assert(zset_rank(&zset, "member3", 7) == 3); // Third element
    assert(zset_rank(&zset, "member4", 7) == 4); // Fourth element

    // Test non-existent member
    assert(zset_rank(&zset, "nonexistent", 11) == 0);

    zset_clear(&zset);
    std::cout << "✓ zset_rank tests passed" << std::endl;
}

void test_zset_nth() {
    std::cout << "Testing zset_nth..." << std::endl;

    ZSet zset = {};

    // Add test data
    assert(zset_insert(&zset, "a", 1, 1.0));
    assert(zset_insert(&zset, "b", 1, 2.0));
    assert(zset_insert(&zset, "c", 1, 3.0));

    // Test nth element retrieval
    ZNode* node1 = zset_nth(&zset, 1);
    assert(node1 && node1->score == 1.0 && strncmp(node1->name, "a", 1) == 0);

    ZNode* node2 = zset_nth(&zset, 2);
    assert(node2 && node2->score == 2.0 && strncmp(node2->name, "b", 1) == 0);

    ZNode* node3 = zset_nth(&zset, 3);
    assert(node3 && node3->score == 3.0 && strncmp(node3->name, "c", 1) == 0);

    // Test out of bounds
    assert(zset_nth(&zset, 0) == nullptr);
    assert(zset_nth(&zset, 4) == nullptr);

    zset_clear(&zset);
    std::cout << "✓ zset_nth tests passed" << std::endl;
}

void test_zset_card() {
    std::cout << "Testing zset_card..." << std::endl;

    ZSet zset = {};

    // Empty zset
    assert(zset_card(&zset) == 0);

    // Add elements
    assert(zset_insert(&zset, "one", 3, 1.0));
    assert(zset_card(&zset) == 1);

    assert(zset_insert(&zset, "two", 3, 2.0));
    assert(zset_card(&zset) == 2);

    assert(zset_insert(&zset, "three", 5, 3.0));
    assert(zset_card(&zset) == 3);

    // Update existing (shouldn't change count)
    assert(!zset_insert(&zset, "one", 3, 1.5));
    assert(zset_card(&zset) == 3);

    zset_clear(&zset);
    std::cout << "✓ zset_card tests passed" << std::endl;
}

void test_zset_range() {
    std::cout << "Testing zset_range..." << std::endl;

    ZSet zset = {};

    // Add test data
    assert(zset_insert(&zset, "alpha", 5, 1.0));
    assert(zset_insert(&zset, "beta", 4, 2.0));
    assert(zset_insert(&zset, "gamma", 5, 3.0));
    assert(zset_insert(&zset, "delta", 5, 4.0));
    assert(zset_insert(&zset, "epsilon", 7, 5.0));

    ZNode* results[10];

    // Test full range
    int count = zset_range(&zset, 0, 5, results);
    assert(count == 5);
    assert(results[0]->score == 1.0);
    assert(results[4]->score == 5.0);

    // Test partial range
    count = zset_range(&zset, 1, 3, results);
    assert(count == 3);
    assert(results[0]->score == 2.0); // beta
    assert(results[2]->score == 4.0); // delta

    // Test range beyond bounds
    count = zset_range(&zset, 3, 10, results);
    assert(count == 2); // Only delta and epsilon available

    zset_clear(&zset);
    std::cout << "✓ zset_range tests passed" << std::endl;
}

void test_zset_range_by_score() {
    std::cout << "Testing zset_range_by_score..." << std::endl;

    ZSet zset = {};

    // Add test data
    assert(zset_insert(&zset, "low", 3, 1.5));
    assert(zset_insert(&zset, "mid", 3, 2.5));
    assert(zset_insert(&zset, "high", 4, 3.5));
    assert(zset_insert(&zset, "very_high", 9, 4.5));

    ZNode* results[10];

    // Test range that includes all
    int count = zset_range_by_score(&zset, 1.0, 5.0, results, 10);
    assert(count == 4);
    assert(results[0]->score == 1.5);
    assert(results[3]->score == 4.5);

    // Test partial range
    count = zset_range_by_score(&zset, 2.0, 3.0, results, 10);
    assert(count == 1);
    assert(results[0]->score == 2.5);

    // Test empty range
    count = zset_range_by_score(&zset, 5.0, 6.0, results, 10);
    assert(count == 0);

    // Test exact boundaries
    count = zset_range_by_score(&zset, 2.5, 3.5, results, 10);
    assert(count == 2);
    assert(results[0]->score == 2.5);
    assert(results[1]->score == 3.5);

    zset_clear(&zset);
    std::cout << "✓ zset_range_by_score tests passed" << std::endl;
}

void test_zset_complex_scenarios() {
    std::cout << "Testing complex ZSet scenarios..." << std::endl;

    ZSet zset = {};

    // Test with duplicate scores
    assert(zset_insert(&zset, "alice", 5, 100.0));
    assert(zset_insert(&zset, "bob", 3, 100.0));
    assert(zset_insert(&zset, "charlie", 7, 100.0));

    assert(zset_card(&zset) == 3);

    // With same scores, lexicographic order should determine ranking
    ZNode* results[10];
    int count = zset_range(&zset, 0, 3, results);
    assert(count == 3);

    // Test that names are properly sorted lexicographically when scores are equal
    assert(strncmp(results[0]->name, "alice", results[0]->len) == 0);
    assert(strncmp(results[1]->name, "bob", results[1]->len) == 0);
    assert(strncmp(results[2]->name, "charlie", results[2]->len) == 0);

    // Test ranking with duplicate scores
    assert(zset_rank(&zset, "alice", 5) == 1);
    assert(zset_rank(&zset, "bob", 3) == 2);
    assert(zset_rank(&zset, "charlie", 7) == 3);

    zset_clear(&zset);
    std::cout << "✓ Complex ZSet scenarios passed" << std::endl;
}

void test_edge_cases() {
    std::cout << "Testing edge cases..." << std::endl;

    ZSet zset = {};

    // Test with empty zset
    assert(zset_card(&zset) == 0);
    assert(zset_rank(&zset, "test", 4) == 0);
    assert(zset_nth(&zset, 1) == nullptr);

    ZNode* results[10];
    assert(zset_range(&zset, 0, 10, results) == 0);
    assert(zset_range_by_score(&zset, 0.0, 100.0, results, 10) == 0);

    // Test with single element
    assert(zset_insert(&zset, "single", 6, 42.0));
    assert(zset_card(&zset) == 1);
    assert(zset_rank(&zset, "single", 6) == 1);

    ZNode* single = zset_nth(&zset, 1);
    assert(single && single->score == 42.0);

    assert(zset_range(&zset, 0, 1, results) == 1);
    assert(zset_range_by_score(&zset, 40.0, 50.0, results, 10) == 1);

    zset_clear(&zset);
    std::cout << "✓ Edge cases passed" << std::endl;
}

int main() {
    std::cout << "Running comprehensive ZSet API tests..." << std::endl;

    test_zset_rank();
    test_zset_nth();
    test_zset_card();
    test_zset_range();
    test_zset_range_by_score();
    test_zset_complex_scenarios();
    test_edge_cases();

    std::cout << "\n🎉 All ZSet API tests passed!" << std::endl;
    return 0;
}