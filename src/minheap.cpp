#include "../include/minheap.h"
#include "../include/db.h"

#include <algorithm>

static constexpr size_t kInvalidIndex = static_cast<size_t>(-1);

static size_t& entry_index(Entry* entry) {
    return entry->expire_heap_idx;
}

void ExpireMinHeap::swap_nodes(size_t a, size_t b) {
    if (a == b) {
        return;
    }
    std::swap(heap_[a], heap_[b]);
    entry_index(heap_[a].entry) = a;
    entry_index(heap_[b].entry) = b;
}

void ExpireMinHeap::sift_up(size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (heap_[parent].expire_at <= heap_[idx].expire_at) {
            break;
        }
        swap_nodes(parent, idx);
        idx = parent;
    }
}

void ExpireMinHeap::sift_down(size_t idx) {
    size_t n = heap_.size();
    while (true) {
        size_t left = idx * 2 + 1;
        size_t right = idx * 2 + 2;
        size_t smallest = idx;
        if (left < n && heap_[left].expire_at < heap_[smallest].expire_at) {
            smallest = left;
        }
        if (right < n && heap_[right].expire_at < heap_[smallest].expire_at) {
            smallest = right;
        }
        if (smallest == idx) {
            break;
        }
        swap_nodes(idx, smallest);
        idx = smallest;
    }
}

void ExpireMinHeap::add_or_update(Entry* entry, int64_t expire_at) {
    if (!entry) {
        return;
    }
    size_t& idx = entry_index(entry);
    if (idx == kInvalidIndex) {
        idx = heap_.size();
        heap_.push_back({entry, expire_at});
        sift_up(idx);
    } else {
        heap_[idx].expire_at = expire_at;
        sift_down(idx);
        sift_up(idx);
    }
}

void ExpireMinHeap::remove(Entry* entry) {
    if (!entry) {
        return;
    }
    size_t& idx_ref = entry_index(entry);
    if (idx_ref == kInvalidIndex) {
        return;
    }
    size_t idx = idx_ref;
    size_t last = heap_.size() - 1;
    if (idx != last) {
        swap_nodes(idx, last);
    }
    heap_.pop_back();
    idx_ref = kInvalidIndex;
    if (idx < heap_.size()) {
        sift_down(idx);
        sift_up(idx);
    }
}

HeapItem* ExpireMinHeap::peek() {
    if (heap_.empty()) {
        return nullptr;
    }
    return &heap_.front();
}

const HeapItem* ExpireMinHeap::peek() const {
    if (heap_.empty()) {
        return nullptr;
    }
    return &heap_.front();
}

HeapItem ExpireMinHeap::pop() {
    HeapItem top = heap_.front();
    remove(top.entry);
    return top;
}

bool ExpireMinHeap::empty() const {
    return heap_.empty();
}
