#pragma once

#include <cassert>
#include <thread>
#include <mutex>

#ifdef _WIN32

#include <windows.h>

typedef unsigned int PAGE_ID;

#else
typedef unsigned long long PAGE_ID;

#endif // _WIN32

const size_t MAX_SIZE = 64 * 1024;      // 64kb
const size_t NFREE_LIST = MAX_SIZE / 8; // �����ܹ� 8*1024 ������ÿ�������Ԫ����[8byte,16byte,...,64*1024byte]
const size_t MAX_PAGES = 129;
const size_t PAGE_SHITF = 12; // 4kΪҳ��λ

// * (T**)&xxx������һ����ַ��ֵ����ǰָ��ָ�������ǰ4/8���ֽ���(ȡ����ƽ̨)
inline void *&NextObj(void *obj) {
    return *((void **) obj);
}

/**
 * @brief �洢�ڴ����ĵ�������
 */
class FreeList {
public:
    // ͷ��
    void Push(void *obj) {
        NextObj(obj) = _freelist;
        _freelist = obj;
        ++_num;
    }

    // ͷɾ
    void *Pop() {
        void *obj = _freelist;
        _freelist = NextObj(obj);
        --_num;
        return obj;
    }

    // ���뵽����������
    void PushRange(void *head, void *tail, size_t num) {
        NextObj(tail) = _freelist;
        _freelist = head;
        _num += num;
    }

    // ������������ȡ���ڴ����
    size_t PopRange(void *&start, void *&end, size_t num) {
        size_t actualNum = 0;
        void *prev = nullptr;
        void *cur = _freelist;
        for (; actualNum < num && cur != nullptr; ++actualNum) {
            prev = cur;
            cur = NextObj(cur);
        }

        start = _freelist;
        end = prev;
        _freelist = cur;

        _num -= actualNum;

        return actualNum;
    }

    size_t Num() {
        return _num;
    }

    bool Empty() {
        return _freelist == nullptr;
    }

    void Clear() {
        _freelist = nullptr;
        _num = 0;
    }

private:
    void *_freelist = nullptr;
    size_t _num = 0; // ���������¹ҵĸ���
};

/**
 * @brief ר�ż���ȡ����С��λ�õ���
 */
class SizeClass {
public:
    // ������[1%��10%]���ҵ�����Ƭ�˷�
    // [1, 128]             8byte����     freelist[0,16)
    // [129, 1024]          16byte����    freelist[16,72)
    // [1025, 8*1024]       128byte����   freelist[72,128)
    // [8*1024+1, 64*1024]  1024byte����  freelist[128,1024)
    static size_t _RoundUp(size_t size, size_t alignment) {
        return (size + alignment - 1) & (~(alignment - 1));
    }

    // �����С���㣬����ȡ��
    static inline size_t RoundUp(size_t size) {
        assert(size <= MAX_SIZE);
        if (size <= 128) {
            return _RoundUp(size, 8);
        } else if (size <= 1024) {
            return _RoundUp(size, 16);
        } else if (size <= 8192) {
            return _RoundUp(size, 128);
        } else if (size <= 65536) {
            return _RoundUp(size, 1024);
        }

        return -1;
    }

    static size_t _ListIndex(size_t size, size_t align_shift) {
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    }

    // ӳ�����������λ��
    static size_t ListIndex(size_t size) {
        assert(size <= MAX_SIZE);

        // ÿ�������ж��ٸ���
        static int group_array[4] = {16, 56, 56, 56};
        if (size <= 128) {
            return _ListIndex(size, 3);
        } else if (size <= 1024) {
            return _ListIndex(size - 128, 4) + group_array[0];
        } else if (size <= 8192) {
            return _ListIndex(size - 1024, 7) + group_array[1] + group_array[0];
        } else if (size <= 65536) {
            return _ListIndex(size - 8192, 10) + group_array[2] + group_array[1] + group_array[0];
        }

        return -1;
    }

    // ����һ����CentralCache������ٸ��ڴ����
    static size_t NumMoveSize(size_t size) {
        if (size == 0)
            return 0;

        int num = MAX_SIZE / size;
        if (num < 2)
            num = 2;

        if (num > 512)
            num = 512;

        return num;
    }

    // ����һ����PageCache��ȡ����Span����
    static size_t NumMovePage(size_t size) {
        size_t num = NumMoveSize(size);
        size_t npage = num * size;

        npage >>= 12;
        if (npage == 0)
            npage = 1;

        return npage;
    }
};

/**
 * @brief ����ҳΪ��λ���ڴ���󣬱����Ƿ������ϲ�������ڴ���Ƭ
 */
struct Span {
    PAGE_ID m_page_id = 0;   // ҳ�ţ���Ϊ�ڴ��ǰ�ҳ����ģ�һҳΪ4kb���ڴ��ַ/4kb�õ�Ψһ��ֵҳ�ţ���ͬһҳ�ϵ������ڴ��ҳ�Ŷ���һ����
    PAGE_ID m_page_size = 0; // ��ǰSpanά����ҳ������
    size_t m_obj_size = 0;   // ����Span������ڴ������з���ʹ��
    FreeList m_span_free_list;    // ����ҳ�ڴ������ThreadCache������������������
    int m_use_count = 0;     // �ڴ����ʹ�õ���������ֵΪ0��˵��û�б�ʹ�ã���ʱ���ڴ淵��PageCache

    Span *_next = nullptr;
    Span *_prev = nullptr;
};

/**
 * @brief �洢Span�����˫������ṹ
 */
class SpanList {
public:
    SpanList() {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }

    // �ͷ������ÿ���ڵ�
    ~SpanList() {
        Span *cur = _head->_next;
        while (cur != _head) {
            Span *next = cur->_next;
            delete cur;
            cur = next;
        }
        delete _head;
        _head = nullptr;
    }

    // ��ֹ��������͸�ֵ����
    SpanList(const SpanList &) = delete;

    SpanList &operator=(const SpanList &) = delete;

    Span *Begin() {
        return _head->_next;
    }

    Span *End() {
        return _head;
    }

    void PushFront(Span *newspan) {
        Insert(_head->_next, newspan);
    }

    void PopFront() {
        Erase(_head->_next);
    }

    void PushBack(Span *newspan) {
        Insert(_head, newspan);
    }

    void PopBack() {
        Erase(_head->_prev);
    }

    // ��posλ�õ�ǰ�����һ��newspan
    void Insert(Span *pos, Span *newspan) {
        Span *prev = pos->_prev;

        prev->_next = newspan;
        newspan->_next = pos;
        pos->_prev = newspan;
        newspan->_prev = prev;
    }

    // ɾ��posλ�õĽڵ�
    // ֻ�������˽ڵ㣬��û���ͷŵ������滹���ô�
    void Erase(Span *pos) {
        assert(pos != _head);

        Span *prev = pos->_prev;
        Span *next = pos->_next;

        prev->_next = next;
        next->_prev = prev;
    }

    bool Empty() {
        return Begin() == End();
    }

    void Lock() {
        _mtx.lock();
    }

    void Unlock() {
        _mtx.unlock();
    }

private:
    Span *_head;

    std::mutex _mtx; // Ͱ��
};

/**
 * @brief ��ϵͳ�����ڴ�
 */
inline static void *SystemAllocate(size_t num_page) {
#ifdef _WIN32
    void *ptr = VirtualAlloc(0, num_page * (1 << PAGE_SHITF), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // brk mmap��
#endif
    if (ptr == nullptr)
        throw std::bad_alloc();

    return ptr;
}

/**
 * @brief ��ϵͳ�ͷ��ڴ�
 */
inline static void SystemFree(void *ptr) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
#endif
}