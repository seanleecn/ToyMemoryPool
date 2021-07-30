#pragma once

#include "Common.hpp"
#include "radix.hpp"
#include <iostream>

/**
 * @brief PageCache��CentralCache�����һ�㻺�棬�洢���ڴ�����ҳΪ��λ�洢������
 * CentralCacheû���ڴ����ʱ�������һ��������page�и�ɶ�����С��С���ڴ棬�����CentralCache
 * PageCache�����CentralCache����������span���󣬲��Һϲ����ڵ�ҳ�������ڴ���Ƭ������
 */
class PageCache {
public:
    // ��PageCache�����ڴ�
    Span *_NewSpan(size_t num_page);

    // ��PageCache��ȡspan
    Span *NewSpan(size_t numpage);

    // �ͷ�span�ص�PageCache�����ϲ����ڵ�span
    void ReleaseSpanToPageCache(Span *span);

    Span *GetIdToSpan(PAGE_ID id);

    // ��C++11��ʹ�ú����ڵ� local static �������̰߳�ȫ�ģ�����Ҫ����
    // ����ģʽ
    static PageCache &GetPageCacheInstance() {
        static PageCache inst;
        return inst;
    }

private:
    PageCache() = default;

    PageCache(const PageCache &) = delete;

    // ά��һ��Span�������飬��ҳһ�����Span������һ��
    // ����Span����ÿ��Span��СΪ128ҳ(512kb)
    SpanList m_page_spanLists[MAX_PAGES];

    Radix<Span *> _idSpanRadix;

    std::mutex _mtx;
};

// ʵ��

Span *PageCache::_NewSpan(size_t num_page) {
    // ���ǲ���������num_page��span
    if (!m_page_spanLists[num_page].Empty()) {
        Span *span = m_page_spanLists[num_page].Begin();
        m_page_spanLists[num_page].PopFront();
        return span;
    }

    // �ұ�numpage�����span
    for (size_t i = num_page + 1; i < MAX_PAGES; ++i) {
        if (!m_page_spanLists[i].Empty()) {
            // ����
            Span *span = m_page_spanLists[i].Begin();
            m_page_spanLists[i].PopFront();

            // ��Span������һ����С���ʵ�
            Span *splitspan = new Span;
            splitspan->m_page_id = span->m_page_id + span->m_page_size - num_page;
            splitspan->m_page_size = num_page;

            // ��Щ�ڴ�ҪҪ����ӳ���ϵ
            for (PAGE_ID i = 0; i < num_page; ++i) {
                // cout << "����ӳ��" << splitspan->m_page_id + i << endl;
                _idSpanRadix.insert(splitspan->m_page_id + i, splitspan);
            }

            // ����ʣ������span�������ӵ�PageCache��
            span->m_page_size -= num_page;
            m_page_spanLists[span->m_page_size].PushFront(span);

            return splitspan;
        }
    }

    // û�������span����ϵͳ����MAX_PAGESҳ���ڴ��½�span
    void *ptr = SystemAllocate(MAX_PAGES - 1);

    // Ϊ�µ�span����ҳ�ŵ�span֮���ӳ���ϵ
    Span *bigspan = new Span;
    // ���ݵ�ַ�������ҳ��
    bigspan->m_page_id = (PAGE_ID) ptr >> PAGE_SHITF;
    bigspan->m_page_size = MAX_PAGES - 1;

    for (PAGE_ID i = 0; i < bigspan->m_page_size; ++i) {
        // cout << "����" << bigspan->m_page_id + i << endl;
        _idSpanRadix.insert(bigspan->m_page_id + i, bigspan);
    }

    m_page_spanLists[bigspan->m_page_size].PushFront(bigspan);

    // �ݹ����_NewSpan
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
    // ��ǰ�ϲ�
    while (true) {
        PAGE_ID prevPageId = span->m_page_id - 1;
        auto pre_id = _idSpanRadix.find(prevPageId);
        // ǰ���ҳ�����ڣ������������
        if (pre_id == nullptr) {
            break;
        }
        Span *prevSpan = pre_id->value;
        // ǰһ��Ҳ����ʹ���У����ܺϲ��������������
        if (prevSpan->m_use_count != 0) {
            break;
        }
        // �ϲ�����ڴ�ҳ��������MAX_PAGES�������������
        if (span->m_page_size + prevSpan->m_page_size >= MAX_PAGES) {
            break;
        }
        // �ϲ�����span
        span->m_page_id = prevSpan->m_page_id;
        span->m_page_size += prevSpan->m_page_size;
        // ����span������
        for (PAGE_ID i = 0; i < prevSpan->m_page_size; ++i) {
            // cout << "�ϲ�" << prevSpan->m_page_id + i << endl;
            _idSpanRadix.insert(prevSpan->m_page_id + i, span);
        }

        m_page_spanLists[prevSpan->m_page_size].Erase(prevSpan);
        delete prevSpan;
    }

    // ���ϲ�
    while (true) {
        PAGE_ID nextPageId = span->m_page_id + span->m_page_size;
        auto nextIt = _idSpanRadix.find(nextPageId);
        // ����û��span��
        if (nextIt == nullptr) {
            break;
        }
        // �����span���ڱ�ʹ��
        Span *nextSpan = nextIt->value;
        if (nextSpan->m_use_count != 0) {
            break;
        }
        // �ϲ�����ڴ�ҳ����MAX_PAGES
        if (span->m_page_size + nextSpan->m_page_size >= MAX_PAGES) {
            break;
        }

        span->m_page_size += nextSpan->m_page_size;
        // ����span������
        for (PAGE_ID i = 0; i < nextSpan->m_page_size; ++i) {
            // cout << "���ϲ�" << nextSpan->m_page_id + i << endl;
            _idSpanRadix.insert(nextSpan->m_page_id + i, span);
        }

        m_page_spanLists[nextSpan->m_page_size].Erase(nextSpan);
        delete nextSpan;
    }
    // �ϲ����Ĵ�span�����뵽PageCache��Ӧ��������
    m_page_spanLists[span->m_page_size].PushFront(span);
    _mtx.unlock();
}

// �޸�: ��unordered_map������radix
Span *PageCache::GetIdToSpan(PAGE_ID id) {
    auto it = _idSpanRadix.find(id);
    if (it != nullptr) {
        return it->value;
    } else {
        return nullptr;
    }
}
