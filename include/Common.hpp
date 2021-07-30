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
const size_t NFREE_LIST = MAX_SIZE / 8; // 数组总共 8*1024 个链表，每个链表的元素是[8byte,16byte,...,64*1024byte]
const size_t MAX_PAGES = 129;
const size_t PAGE_SHITF = 12; // 4k为页移位

// * (T**)&xxx可以让一个地址赋值到当前指针指向变量的前4/8个字节上(取决于平台)
inline void *&NextObj(void *obj) {
    return *((void **) obj);
}

/**
 * @brief 存储内存对象的单向链表
 */
class FreeList {
public:
    // 头插
    void Push(void *obj) {
        NextObj(obj) = _freelist;
        _freelist = obj;
        ++_num;
    }

    // 头删
    void *Pop() {
        void *obj = _freelist;
        _freelist = NextObj(obj);
        --_num;
        return obj;
    }

    // 插入到自由链表中
    void PushRange(void *head, void *tail, size_t num) {
        NextObj(tail) = _freelist;
        _freelist = head;
        _num += num;
    }

    // 从自由链表中取走内存对象
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
    size_t _num = 0; // 自由链表下挂的个数
};

/**
 * @brief 专门计算取整大小和位置的类
 */
class SizeClass {
public:
    // 控制在[1%，10%]左右的内碎片浪费
    // [1, 128]             8byte对齐     freelist[0,16)
    // [129, 1024]          16byte对齐    freelist[16,72)
    // [1025, 8*1024]       128byte对齐   freelist[72,128)
    // [8*1024+1, 64*1024]  1024byte对齐  freelist[128,1024)
    static size_t _RoundUp(size_t size, size_t alignment) {
        return (size + alignment - 1) & (~(alignment - 1));
    }

    // 对齐大小计算，向上取整
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

    // 映射自由链表的位置
    static size_t ListIndex(size_t size) {
        assert(size <= MAX_SIZE);

        // 每个区间有多少个链
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

    // 计算一次向CentralCache申请多少个内存对象
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

    // 计算一次向PageCache获取多大的Span对象
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
 * @brief 管理页为单位的内存对象，本质是方便做合并，解决内存碎片
 */
struct Span {
    PAGE_ID m_page_id = 0;   // 页号，因为内存是按页分配的，一页为4kb，内存地址/4kb得到唯一的值页号，且同一页上的所有内存的页号都是一样的
    PAGE_ID m_page_size = 0; // 当前Span维护的页的数量
    size_t m_obj_size = 0;   // 将该Span按多大内存对象进行分配使用
    FreeList m_span_free_list;    // 将该页内存对象用ThreadCache那样的链表连接起来
    int m_use_count = 0;     // 内存对象被使用的数量。当值为0，说明没有被使用，此时把内存返回PageCache

    Span *_next = nullptr;
    Span *_prev = nullptr;
};

/**
 * @brief 存储Span对象的双向链表结构
 */
class SpanList {
public:
    SpanList() {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }

    // 释放链表的每个节点
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

    // 防止拷贝构造和赋值构造
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

    // 在pos位置的前面插入一个newspan
    void Insert(Span *pos, Span *newspan) {
        Span *prev = pos->_prev;

        prev->_next = newspan;
        newspan->_next = pos;
        pos->_prev = newspan;
        newspan->_prev = prev;
    }

    // 删除pos位置的节点
    // 只是跳过了节点，并没有释放掉，后面还有用处
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

    std::mutex _mtx; // 桶锁
};

/**
 * @brief 向系统申请内存
 */
inline static void *SystemAllocate(size_t num_page) {
#ifdef _WIN32
    void *ptr = VirtualAlloc(0, num_page * (1 << PAGE_SHITF), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // brk mmap等
#endif
    if (ptr == nullptr)
        throw std::bad_alloc();

    return ptr;
}

/**
 * @brief 向系统释放内存
 */
inline static void SystemFree(void *ptr) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
#endif
}