#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

struct Entry;

struct HeapItem {
    Entry* entry;
    int64_t expire_at;
};

class ExpireMinHeap {
public:
    void add_or_update(Entry* entry, int64_t expire_at);
    void remove(Entry* entry);
    HeapItem* peek();
    const HeapItem* peek() const;
    HeapItem pop();
    bool empty() const;

private:
    std::vector<HeapItem> heap_;

    void sift_up(size_t idx);
    void sift_down(size_t idx);
    void swap_nodes(size_t a, size_t b);
};
