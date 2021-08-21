#pragma once

#include "Common.hpp"
#include "radix.hpp"
#include <iostream>

/**
 * @brief PageCache是CentralCache上面的一层缓存，存储的内存是以页为单位存储及分配
 * CentralCache没有内存对象时，分配出一定数量的page切割成定长大小的小块内存，分配给CentralCache
 * PageCache会回收CentralCache满足条件的span对象，并且合并相邻的页，缓解内存碎片的问题
 */
class PageCache {
public:
    // 向PageCache申请内存
    Span *_NewSpan(size_t num_page);

    // 从PageCache提取span
    Span *NewSpan(size_t numpage);

    // 释放span回到PageCache，并合并相邻的span
    void ReleaseSpanToPageCache(Span *span);

    Span *GetIdToSpan(PAGE_ID id);

    // 在C++11下使用函数内的 local static 对象是线程安全的，不需要加锁
    // 饿汉模式
    static PageCache &GetPageCacheInstance() {
        static PageCache inst;
        return inst;
    }

private:
    PageCache() = default;

    PageCache(const PageCache &) = delete;

    // 维护一个Span链表数组，将页一样大的Span连接在一起
    // 最大的Span链表每个Span大小为128页(512kb)
    SpanList m_page_spanLists[MAX_PAGES];

    Radix<Span *> _idSpanRadix;

    std::mutex _mtx;
};

// 实现

Span *PageCache::_NewSpan(size_t num_page) {
    // 找是不是有满足num_page的span
    if (!m_page_spanLists[num_page].Empty()) {
        Span *span = m_page_spanLists[num_page].Begin();
        m_page_spanLists[num_page].PopFront();
        return span;
    }

    // 找比numpage更大的span
    for (size_t i = num_page + 1; i < MAX_PAGES; ++i) {
        if (!m_page_spanLists[i].Empty()) {
            // 分裂
            Span *span = m_page_spanLists[i].Begin();
            m_page_spanLists[i].PopFront();

            // 从Span后面切一个大小合适的
            Span *splitspan = new Span;
            splitspan->m_page_id = span->m_page_id + span->m_page_size - num_page;
            splitspan->m_page_size = num_page;

            // 这些内存要要更新映射关系
            for (PAGE_ID i = 0; i < num_page; ++i) {
                // cout << "更新映射" << splitspan->m_page_id + i << endl;
                _idSpanRadix.insert(splitspan->m_page_id + i, splitspan);
            }

            // 将切剩下来的span重新链接到PageCache中
            span->m_page_size -= num_page;
            m_page_spanLists[span->m_page_size].PushFront(span);

            return splitspan;
        }
    }

    // 没有满足的span，向系统申请MAX_PAGES页的内存新建span
    void *ptr = SystemAllocate(MAX_PAGES - 1);

    // 为新的span建立页号到span之间的映射关系
    Span *bigspan = new Span;
    // 根据地址，计算出页号
    bigspan->m_page_id = (PAGE_ID) ptr >> PAGE_SHITF;
    bigspan->m_page_size = MAX_PAGES - 1;

    for (PAGE_ID i = 0; i < bigspan->m_page_size; ++i) {
        // cout << "插入" << bigspan->m_page_id + i << endl;
        _idSpanRadix.insert(bigspan->m_page_id + i, bigspan);
    }

    m_page_spanLists[bigspan->m_page_size].PushFront(bigspan);

    // 递归调用_NewSpan
    return _NewSpan(num_page);
}

Span *PageCache::NewSpan(size_t numpage) {
    _mtx.lock();
    Span *span = _NewSpan(numpage);
    _mtx.unlock();
    return span;
}

void PageCache::ReleaseSpanToPageCache(Span *span) {
    _mtx.lock();
    // 向前合并
    while (true) {
        PAGE_ID prevPageId = span->m_page_id - 1;
        auto pre_id = _idSpanRadix.find(prevPageId);
        // 前面的页不存在，进入向后搜索
        if (pre_id == nullptr) {
            break;
        }
        Span *prevSpan = pre_id->value;
        // 前一个也还在使用中，不能合并，进入向后搜索
        if (prevSpan->m_use_count != 0) {
            break;
        }
        // 合并后的内存页数量大于MAX_PAGES，进入向后搜索
        if (span->m_page_size + prevSpan->m_page_size >= MAX_PAGES) {
            break;
        }
        // 合并两个span
        span->m_page_id = prevSpan->m_page_id;
        span->m_page_size += prevSpan->m_page_size;
        // 更新span的索引
        for (PAGE_ID i = 0; i < prevSpan->m_page_size; ++i) {
            // cout << "合并" << prevSpan->m_page_id + i << endl;
            _idSpanRadix.insert(prevSpan->m_page_id + i, span);
        }

        m_page_spanLists[prevSpan->m_page_size].Erase(prevSpan);
        delete prevSpan;
    }

    // 向后合并
    while (true) {
        PAGE_ID nextPageId = span->m_page_id + span->m_page_size;
        auto nextIt = _idSpanRadix.find(nextPageId);
        // 后面没有span了
        if (nextIt == nullptr) {
            break;
        }
        // 后面的span还在被使用
        Span *nextSpan = nextIt->value;
        if (nextSpan->m_use_count != 0) {
            break;
        }
        // 合并后的内存页大于MAX_PAGES
        if (span->m_page_size + nextSpan->m_page_size >= MAX_PAGES) {
            break;
        }

        span->m_page_size += nextSpan->m_page_size;
        // 更新span的索引
        for (PAGE_ID i = 0; i < nextSpan->m_page_size; ++i) {
            // cout << "向后合并" << nextSpan->m_page_id + i << endl;
            _idSpanRadix.insert(nextSpan->m_page_id + i, span);
        }

        m_page_spanLists[nextSpan->m_page_size].Erase(nextSpan);
        delete nextSpan;
    }
    // 合并出的大span，插入到PageCache对应的链表中
    m_page_spanLists[span->m_page_size].PushFront(span);
    _mtx.unlock();
}

// 修改: 把unordered_map换成了radix
Span *PageCache::GetIdToSpan(PAGE_ID id) {
    auto it = _idSpanRadix.find(id);
    if (it != nullptr) {
        return it->value;
    } else {
        return nullptr;
    }
}
