#pragma once

#include "Common.hpp"
#include "PageCache.hpp"

/**
 * @brief CentralCache是所有线程所共享
 * ThreadCache按需从CentralCache中获取的对象，CentralCache周期性回收ThreadCache中的对象
 */
class CentralCache
{
public:
    // 从中心缓存获取一定数量的对象给ThreadCache
    size_t FetchRangeObj(void *&start, void *&end, size_t num, size_t size);

    // 将一定数量的内存对象释放到Span
    void ReleseListToSpans(void *start, size_t size);

    // 从_spanlists或者PageCache获取一个span
    Span *GetOneSpan(size_t size);

    // 在C++11下使用函数内的 local static 对象是线程安全的，不需要加锁
    // 懒汉模式
    static CentralCache &GetCentralCacheInstance()
    {
        static CentralCache inst;
        return inst;
    }

private:
    CentralCache() = default;

    CentralCache(const CentralCache &) = delete;

    // CentralCache是存在竞争的，所以从这里取内存对象是需要加锁（桶锁）
    SpanList _spanlists[NFREE_LIST];
};

// 实现

Span *CentralCache::GetOneSpan(size_t size)
{
    size_t index = SizeClass::ListIndex(size);
    SpanList &spanlist = _spanlists[index];
    Span *it = spanlist.Begin();
    // spanlist中有空余的内存对象span
    while (it != spanlist.End())
    {
        if (!it->m_span_free_list.Empty())
        {
            return it;
        }
        else
        {
            it = it->_next;
        }
    }

    // 计算要向PageCache获取大小为多少页的span
    size_t num_page = SizeClass::NumMovePage(size);

    // 从PageCache 获取一个span
    Span *span = PageCache::GetPageCacheInstance().NewSpan(num_page);

    // 把span对象切成对应大小挂到span的freelist中
    char *start = (char *)(span->m_page_id << PAGE_SHITF);
    char *end = start + (span->m_page_size << PAGE_SHITF);
    while (start < end)
    {
        char *obj = start;
        start += size;

        span->m_span_free_list.Push(obj);
    }
    span->m_obj_size = size;

    // 将获取的Span插入CentralCache
    spanlist.PushFront(span);

    return span;
}

size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t num, size_t size)
{
    // 计算所需内存对象的链表在CentralCache中的下标
    size_t index = SizeClass::ListIndex(size);
    SpanList &spanlist = _spanlists[index];
    // 注意加锁
    spanlist.Lock();
    // 从span链表中获取一个span
    Span *span = GetOneSpan(size);
    FreeList &s_freelist = span->m_span_free_list;
    // PopRange参数是输出参数
    size_t actualNum = s_freelist.PopRange(start, end, num);
    span->m_use_count += actualNum;

    spanlist.Unlock();

    return actualNum; // 返回实际获取到的内存对象数量
}

void CentralCache::ReleseListToSpans(void *start, size_t size)
{
    // 计算下标
    size_t index = SizeClass::ListIndex(size);
    SpanList &spanlist = _spanlists[index];
    spanlist.Lock();

    // 让next保存start的下一个内存对象，并移动start直到指向空
    while (start)
    {
        void *next = NextObj(start);
        // 找start内存块属于哪个span
        PAGE_ID id = (PAGE_ID)start >> PAGE_SHITF;                      // 对象链表地址右移12位得到页号
        Span *span = PageCache::GetPageCacheInstance().GetIdToSpan(id); // 用页号查找span
        span->m_span_free_list.Push(start);
        span->m_use_count--;

        // 当前span切出去的对象全部返回，可以将Span还给PageCache
        if (span->m_use_count == 0)
        {
            // 得到下标
            size_t index = SizeClass::ListIndex(span->m_obj_size);
            _spanlists[index].Erase(span);
            span->m_span_free_list.Clear();

            PageCache::GetPageCacheInstance().ReleaseSpanToPageCache(span);
        }

        start = next;
    }

    spanlist.Unlock();
}