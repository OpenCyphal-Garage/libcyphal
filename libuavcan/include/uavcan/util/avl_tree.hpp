/*
* AVL Tree implementation.
* Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
*/

#ifndef UAVCAN_UTIL_AVL_TREE_HPP_INCLUDED
#define UAVCAN_UTIL_AVL_TREE_HPP_INCLUDED

#include <cstdlib>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <functional>
#include <uavcan/build_config.hpp>
#include <uavcan/util/templates.hpp>
#include <uavcan/dynamic_memory.hpp>
#include <uavcan/debug.hpp>

namespace uavcan {

template<typename T>
class UAVCAN_EXPORT AvlTree : Noncopyable {
protected:
    struct Node {
        T *data;
        int h = 1; // initially added as leaf
        Node *left = UAVCAN_NULLPTR;
        Node *right = UAVCAN_NULLPTR;
        Node *equalKeys = UAVCAN_NULLPTR;
    };

    Node *root_;

private:
    size_t len_ ;

    int heightOf(const Node *n) const{
        if(n == UAVCAN_NULLPTR){
            return 0;
        }

        return n->h;
    }

    void inOrderTraverseRecursively(Node *n, std::function<void(T *&)> forEach) {
        if (n == UAVCAN_NULLPTR) {
            return;
        }

        inOrderTraverseRecursively(n->left, forEach);
        forEach(n->data);
        inOrderTraverseRecursively(n->right, forEach);
    }

    Node *makeNode(T *payload) {
        void *praw = this->allocator_.allocate(sizeof(Node));

        if (praw == UAVCAN_NULLPTR) {
            UAVCAN_TRACE("AvlTree", " OOM -- Can't allocate Node");
            return UAVCAN_NULLPTR; // Push rejected
        }

        Node *node = new (praw) Node();
        UAVCAN_ASSERT(node);

        node->data = payload;
        return node;
    }

    void deleteNode(Node *&n) {
        if (n != UAVCAN_NULLPTR) {
            n->~Node();
            allocator_.deallocate(n);
            n = UAVCAN_NULLPTR;
        }
    }

    int balanceOf(Node *n) const {
        if (n == UAVCAN_NULLPTR) {
            return 0;
        }

        return heightOf(n->left) - heightOf(n->right);
    }

    Node *rotateRight(Node *y) const {
        Node *x = y->left;
        Node *T2 = x->right;

        x->right = y;
        y->left = T2;

        y->h = std::max(heightOf(y->left), heightOf(y->right)) + 1;
        x->h = std::max(heightOf(x->left), heightOf(x->right)) + 1;

        return x;
    }

    Node *rotateLeft(Node *x) {
        Node *y = x->right;
        Node *T2 = y->left;

        y->left = x;
        x->right = T2;

        x->h = std::max(heightOf(x->left), heightOf(x->right)) + 1;
        y->h = std::max(heightOf(y->left), heightOf(y->right)) + 1;

        return y;
    }

    // If UAVCAN_NULLPTR is returned, OOM happened.
    Node *insert_helper(Node *node, Node *newNode) {
        if (node == UAVCAN_NULLPTR) {
            len_++;
            return newNode;
        }

        if (*newNode->data < *node->data) {
            node->left = insert_helper(node->left, newNode);
        } else if (*newNode->data > *node->data) {
            node->right = insert_helper(node->right, newNode);
        } else {
            len_++;
            appendToEndOf(node, newNode);
            return node;
        }

        node->h = std::max(heightOf(node->left), heightOf(node->right)) + 1;

        int balance = balanceOf(node);

        if (balance > 1 && *newNode->data < *node->left->data) {
            return rotateRight(node);
        }

        if (balance < -1 && *newNode->data > *node->right->data) {
            return rotateLeft(node);
        }

        if (balance > 1 && *newNode->data > *node->left->data) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }

        if (balance < -1 && *newNode->data < *node->right->data) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }

        return node;
    }

    void appendToEndOf(Node *head, Node *newNode){
        Node *target = head;
        while(target->equalKeys != UAVCAN_NULLPTR){
            target = head->equalKeys;
        }

        target->equalKeys = newNode;
    }

    /* Delete the element of the linked list (memory address comparison)
     * and return the new head */
    Node *deleteFromList(Node *root, T *data){
        auto current = root;
        Node *prev = UAVCAN_NULLPTR;

        while(current != UAVCAN_NULLPTR){
            if(current->data == data){
                if(current == root){
                    auto ret = current->equalKeys;
                    len_--;
                    deleteNode(current);
                    return ret; /* Return one element forward */
                }else{
                    auto next = current->equalKeys;
                    prev->equalKeys = next;
                    len_--;
                    deleteNode(current);
                    return root; /* Unchanged root, non-head element was changed */
                }
            }

            prev = current;
            current = current->equalKeys;
        }

        return root;
    }

protected:
    /*
     * Use this only to allocate the Node struct.
     * `T data` should be already allocated and
     * provided ready for usage from the outside world
     * */
    LimitedPoolAllocator allocator_;

    void postOrderNodeTraverseRecursively(Node *n, std::function<void(Node *&)> forEach) {
        if (n == UAVCAN_NULLPTR) {
            return;
        }

        postOrderNodeTraverseRecursively(n->left, forEach);
        postOrderNodeTraverseRecursively(n->right, forEach);
        forEach(n);
    }

    /* If we've got a Node*, avoid the dereference data equality checks and proceed to the dealloc logic */
    Node *remove_always(Node *node, T *data){
        if (node == UAVCAN_NULLPTR) {
            return node;
        }

        // Stripped out the equality checks
        if(node->equalKeys == UAVCAN_NULLPTR){
            if (node->left == UAVCAN_NULLPTR || node->right == UAVCAN_NULLPTR) {
                Node *temp = node->left ? node->left : node->right;

                if (temp == UAVCAN_NULLPTR) {
                    temp = node;
                    node = UAVCAN_NULLPTR;
                } else {
                    *node = *temp;
                }

                len_--;
                deleteNode(temp);
            } else {
                Node *minOfRight = node->right;
                Node *next = minOfRight->left;

                while (next != UAVCAN_NULLPTR){
                    minOfRight = next->left;
                    next = minOfRight->left;
                }

                node->data = minOfRight->data;
                node->right = remove_helper(node->right, minOfRight->data);
            }
        }else{
            auto newHead = deleteFromList(node, data);
            return newHead;
        }

        if (node == UAVCAN_NULLPTR) {
            return node;
        }

        node->h = std::max(heightOf(node->left),
                           heightOf(node->right)) + 1;

        int balance = balanceOf(node);

        if (balance > 1 && balanceOf(node->left) >= 0) {
            return rotateRight(node);
        }

        if (balance > 1 && balanceOf(node->left) < 0) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }

        if (balance < -1 && balanceOf(node->right) <= 0) {
            return rotateLeft(node);
        }

        if (balance < -1 && balanceOf(node->right) > 0) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }

        return node;
    }

    Node *remove_helper(Node *node, T *data) {
        if (node == UAVCAN_NULLPTR) {
            return node;
        }

        if (*data < *node->data) {
            node->left = remove_helper(node->left, data);
        } else if (*data > *node->data) {
            node->right = remove_helper(node->right, data);
        } else {
            if(node->equalKeys == UAVCAN_NULLPTR){
                if (node->left == UAVCAN_NULLPTR || node->right == UAVCAN_NULLPTR) {
                    Node *temp = node->left ? node->left : node->right;

                    if (temp == UAVCAN_NULLPTR) {
                        temp = node;
                        node = UAVCAN_NULLPTR;
                    } else {
                        *node = *temp;
                    }

                    len_--;
                    deleteNode(temp);
                } else {
                    Node *minOfRight = node->right;
                    Node *next = minOfRight->left;

                    while (next != UAVCAN_NULLPTR){
                        minOfRight = next->left;
                        next = minOfRight->left;
                    }

                    node->data = minOfRight->data;
                    node->right = remove_helper(node->right, minOfRight->data);
                }
            }else{
                auto newHead = deleteFromList(node, data);
                return newHead;
            }
        }

        if (node == UAVCAN_NULLPTR) {
            return node;
        }

        node->h = std::max(heightOf(node->left),
                           heightOf(node->right)) + 1;

        int balance = balanceOf(node);

        if (balance > 1 && balanceOf(node->left) >= 0) {
            return rotateRight(node);
        }

        if (balance > 1 && balanceOf(node->left) < 0) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }

        if (balance < -1 && balanceOf(node->right) <= 0) {
            return rotateLeft(node);
        }

        if (balance < -1 && balanceOf(node->right) > 0) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }

        return node;
    }

    bool linkedListContains(Node *head, const T *data) const{
        Node *next = head;
        while(next != UAVCAN_NULLPTR){
            if(next->data == data){ /* Memory address comparison */
                return true;
            }
            next = next->equalKeys;
        }
        return false;
    }

public:
    AvlTree(IPoolAllocator &allocator, std::size_t allocator_quota):
    root_(UAVCAN_NULLPTR),
    len_(0),
    allocator_(allocator, allocator_quota){}

    virtual ~AvlTree() {
        // delete leafs first
        postOrderNodeTraverseRecursively(root_, [this](Node*& n){
            this->deleteNode(n);
        });
    }

    void remove_entry(T *data){
        root_ = remove_helper(root_, data);
    }

    bool insert(T *data) {
        Node *newNode = makeNode(data);
        if(newNode == UAVCAN_NULLPTR){
            return false;
        }

        root_ = insert_helper(root_, newNode);

        return true;
    }

    size_t getSize() const {
        return root_ == UAVCAN_NULLPTR ? 0 : len_;
    }

    void walk(std::function<void(T*&)> forEach){
        inOrderTraverseRecursively(root_, forEach);
    }

    bool isEmpty() const {
        return getSize() == 0;
    }

    T *max() const {
        Node *n = root_;

        if (n == UAVCAN_NULLPTR) {
            return UAVCAN_NULLPTR;
        }

        for (;;) {
            Node *next = n->right;
            if (next == UAVCAN_NULLPTR) {
                return n->data;
            }

            n = next;
        }
    }

    bool contains(const T *data) const{
        Node *n = root_;
        while (n != UAVCAN_NULLPTR) {
            if(*n->data < *data){
                n = n->right;
                continue;
            }

            if(*n->data > *data){
                n = n->left;
                continue;
            }

            return linkedListContains(n, data);
        }
        return false;
    }

};
}

#endif // UAVCAN_UTIL_AVL_TREE_HPP_INCLUDED
