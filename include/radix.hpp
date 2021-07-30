#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <boost\pool\pool.hpp>
#include <stddef.h>

using namespace std;

#define RADIX_INSERT_VALUE_OCCUPY -1 // �ýڵ��ѱ�ռ��
#define RADIX_INSERT_VALUE_SAME -2     // ��������ͬ��ֵ
#define RADIX_DELETE_ERROR -3         // ɾ������

#define BITS 2
const int radix_tree_height = sizeof(int) * 8 / BITS; // ���ĸ߶�

// ����key����posָ����λ��ֵ��λ����BITSָ����ÿ�λ�ȡ��λ�����Ƶ�ֵ
#define CHECK_BITS(key, pos) ((((unsigned int)(key)) << sizeof(int) * 8 - ((pos) + 1) * BITS) >> (sizeof(int) * 8 - BITS))

// �������ڵ�
template<class T>
struct radix_node_t {
    radix_node_t<T> *child[4]; // �ĸ�ָ���²�ڵ��ָ�����飬�ֱ�ָ��ڵ㴢��ֵΪ00 01 10 11�Ľڵ㡣
    radix_node_t<T> *parent;   // ��¼�����
    T value;                   // �ڵ㴢���ֵ
};

// ������ͷ�ڵ�
template<class T>
struct radix_head {
    // ���ڵ�
    radix_node_t<T> *root;
    // �����ѷ��䵫�������еĽڵ㣨˫���������ﴢ�����е�һ���ڵ㣩
    size_t number; //�洢Ŀǰ�ж��ٸ�������Ľڵ㡣
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
            // �ӵ�λ��ʼ��ȡÿ��λ��ֵ
            temp = CHECK_BITS(key, i);

            if (node->child[temp] == nullptr) {
                child = (radix_node_t<T> *) _pool.malloc();
                if (!child)
                    return NULL;

                // ��ʽ����
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
        // ������1
        ++t->number;
        return 0;
    }

    int Delete(k_type key) {
        radix_node_t<T> *tar = find(key);
        if (tar == nullptr) {
            cout << "ɾ��ʧ��" << endl;
            return RADIX_DELETE_ERROR; //����-3��ɾ��ʧ�ܣ�û�и�����
        } else {
            radix_node_t<T> *par = tar->parent;
            //printf("����ɾ��%d\n", tar->value->m_page_id);
            tar->value = v_type();
            _pool.free(tar);
            --t->number;
            for (int i = 0; i < 4; i++) {
                if (par->child[i] == tar) {
                    par->child[i] = nullptr;
                    break;
                }
            }
            //printf("���ٵ�%d��\n", t->number);
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
