#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <boost\pool\pool.hpp>
#include <stddef.h>

using namespace std;

#define RADIX_INSERT_VALUE_OCCUPY -1 // 该节点已被占用
#define RADIX_INSERT_VALUE_SAME -2     // 插入了相同的值
#define RADIX_DELETE_ERROR -3         // 删除错误

#define BITS 2
const int radix_tree_height = sizeof(int) * 8 / BITS; // 树的高度

// 返回key中由pos指定的位的值，位数由BITS指定，每次获取两位二进制的值
#define CHECK_BITS(key, pos) ((((unsigned int)(key)) << sizeof(int) * 8 - ((pos) + 1) * BITS) >> (sizeof(int) * 8 - BITS))

// 基数树节点
template<class T>
struct radix_node_t {
    radix_node_t<T> *child[4]; // 四个指向下层节点的指针数组，分别指向节点储存值为00 01 10 11的节点。
    radix_node_t<T> *parent;   // 记录父结点
    T value;                   // 节点储存的值
};

// 基数树头节点
template<class T>
struct radix_head {
    // 根节点
    radix_node_t<T> *root;
    // 储存已分配但不在树中的节点（双向链表，这里储存其中的一个节点）
    size_t number; //存储目前有多少个存进来的节点。
};

template<class T>
class Radix {
public:
    typedef int k_type;
    typedef T v_type;

    Radix()
            : _pool(sizeof(radix_node_t<T>)) {
        t = (radix_head<T> *) malloc(sizeof(radix_head<T>));
        t->number = 0;
        t->root = (radix_node_t<T> *) _pool.malloc();
        for (int i = 0; i < 4; i++)
            t->root->child[i] = nullptr;
        t->root->value = T();
    }

    int insert(k_type key, v_type value) {
        int i, temp;
        radix_node_t<T> *node, *child;
        node = t->root;
        for (i = 0; i < radix_tree_height; i++) {
            // 从低位开始获取每两位的值
            temp = CHECK_BITS(key, i);

            if (node->child[temp] == nullptr) {
                child = (radix_node_t<T> *) _pool.malloc();
                if (!child)
                    return NULL;

                // 显式构造
                for (int i = 0; i < 4; i++)
                    child->child[i] = nullptr;
                child->value = v_type();

                child->parent = node;
                node->child[temp] = child;
                node = node->child[temp];
            } else {
                node = node->child[temp];
            }
        }

        node->value = value;
        // 计数加1
        ++t->number;
        return 0;
    }

    int Delete(k_type key) {
        radix_node_t<T> *tar = find(key);
        if (tar == nullptr) {
            cout << "删除失败" << endl;
            return RADIX_DELETE_ERROR; //返回-3，删除失败，没有该数据
        } else {
            radix_node_t<T> *par = tar->parent;
            //printf("真正删除%d\n", tar->value->m_page_id);
            tar->value = v_type();
            _pool.free(tar);
            --t->number;
            for (int i = 0; i < 4; i++) {
                if (par->child[i] == tar) {
                    par->child[i] = nullptr;
                    break;
                }
            }
            //printf("减少到%d个\n", t->number);
        }
        return 0;
    }

    void print() {
        radix_node_t<T> *node = t->root;
        _radix_print(node);
    }

    radix_node_t<T> *find(k_type key) {
        int i = 0, temp;
        radix_node_t<T> *node;
        node = t->root;
        while (node && i < radix_tree_height) {
            temp = CHECK_BITS(key, i++);
            node = node->child[temp];
        }
        if (node == nullptr || node->value == v_type())
            return nullptr;
        return node;
    }

private:
    void _radix_print(radix_node_t<T> *node) {
        if (node == nullptr)
            return;
        if (node->value != v_type())
            printf("%s\n", node->value);
        _radix_print(node->child[0]);
        _radix_print(node->child[1]);
        _radix_print(node->child[2]);
        _radix_print(node->child[3]);
    }

    boost::pool<> _pool;
    radix_head<T> *t;
};
