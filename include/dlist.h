#pragma once

#include <cstddef>

struct ListNode {
    ListNode* prev;
    ListNode* next;
};

inline void list_node_init(ListNode* node) {
    if (!node) {
        return;
    }
    node->prev = node;
    node->next = node;
}

inline void list_head_init(ListNode* head) {
    list_node_init(head);
}

inline bool list_node_linked(const ListNode* node) {
    if (!node) {
        return false;
    }
    return !(node->next == node && node->prev == node);
}

inline bool list_is_empty(const ListNode* head) {
    if (!head) {
        return true;
    }
    return head->next == head;
}

inline void list_insert_before(ListNode* pos, ListNode* node) {
    if (!pos || !node) {
        return;
    }
    ListNode* prev = pos->prev;
    node->next = pos;
    node->prev = prev;
    prev->next = node;
    pos->prev = node;
}

inline void list_remove(ListNode* node) {
    if (!node || !list_node_linked(node)) {
        return;
    }
    ListNode* prev = node->prev;
    ListNode* next = node->next;
    prev->next = next;
    next->prev = prev;
    list_node_init(node);
}

inline void list_push_back(ListNode* head, ListNode* node) {
    if (!head || !node) {
        return;
    }
    if (list_node_linked(node)) {
        list_remove(node);
    }
    ListNode* tail = head->prev;
    node->next = head;
    node->prev = tail;
    tail->next = node;
    head->prev = node;
}

inline ListNode* list_front(ListNode* head) {
    if (!head || list_is_empty(head)) {
        return nullptr;
    }
    return head->next;
}

inline const ListNode* list_front(const ListNode* head) {
    if (!head || list_is_empty(head)) {
        return nullptr;
    }
    return head->next;
}
