#include "Common.hpp"
#include "CentralCache.hpp"

/**
 * @brief 线程内存缓存，使用thread local storage(TLS)保证线程独有
 * 用于小于64k的内存分配，分配不需要加锁保证高效
 */
class ThreadCache {
public:
    // 申请内存
    void *Allocate(size_t size);

    // 释放内存
    void deAllocate(void *ptr, size_t size);

    // 从CentralCache获取对象
    void *FetchFromCentralCache(size_t index);

    // 如果freeList超过一定长度就要释放给CentralCache
    void ListTooLong(FreeList &freeList, size_t num, size_t size);

private:
    // 维护一个FreeList数组，每个 [8bytes,16bytes,...,64k]
    // 可以通过数组下标获得链表对应的大小内存
    FreeList m_thread_freeLists[FREE_LIST_SIZE];
};

// 静态TSL 在每个线程中为其申请空间
_declspec(thread)
static ThreadCache *pThreadCache = nullptr;

//实现

void *ThreadCache::Allocate(size_t size) {
    // 计算所需要的内存对象在ThreadCache中的下标
    size_t index = SizeClass::ListIndex(size);
    FreeList &freeList = m_thread_freeLists[index];
    // freeList不为空直接返回
    if (!freeList.Empty()) {
        return freeList.Pop();
    }
        // 否则从CentralCache中获取内存对象
    else {
        return FetchFromCentralCache(SizeClass::RoundUp(size));
    }
}

void ThreadCache::deAllocate(void *ptr, size_t size) {
    // 计算释放的内存对象在ThreadCache中的下标
    size_t index = SizeClass::ListIndex(size);
    FreeList &freeList = m_thread_freeLists[index];
    // 把释放的内存对象插入到链表中
    freeList.Push(ptr);

    // 检查当前链表是不是过长(链表内存之和大于64kb)
    size_t num = SizeClass::NumMoveSize(size);
    if (freeList.Num() >= num) {
        // 把链表中的全部内存对象释放回CentralCache中
        ListTooLong(freeList, num, size);
    }
}

// 如果freeList超过一定长度就要释放给CentralCache
void ThreadCache::ListTooLong(FreeList &freeList, size_t num, size_t size) {
    void *start = nullptr;
    void *end = nullptr;

    freeList.PopRange(start, end, num);

    NextObj(end) = nullptr;
    // ReleseListToSpans将对象释放回CentralCache的span中
    CentralCache::GetCentralCacheInstance().ReleaseListToSpans(start, size);
}

// 从中心缓存获取span
void *ThreadCache::FetchFromCentralCache(size_t size) {
    size_t num = SizeClass::NumMoveSize(size);

    void *start = nullptr, *end = nullptr;

    // 实际从CentralCache中获取了几个对象
    // start end是返回参数
    size_t actualNum = CentralCache::GetCentralCacheInstance().FetchRangeObj(start, end, num, size);

    // 将获取到的多的内存对象添加到链表中，并返回第一个内存对象
    if (actualNum == 1) {
        return start;
    } else {
        size_t index = SizeClass::ListIndex(size);
        FreeList &list = m_thread_freeLists[index];
        list.PushRange(NextObj(start), end, actualNum - 1);
        return start;
    }
}
