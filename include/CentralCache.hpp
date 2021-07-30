#pragma once

#include "Common.hpp"
#include "PageCache.hpp"

/**
 * @brief CentralCache�������߳�������
 * ThreadCache�����CentralCache�л�ȡ�Ķ���CentralCache�����Ի���ThreadCache�еĶ���
 */
class CentralCache
{
public:
    // �����Ļ����ȡһ�������Ķ����ThreadCache
    size_t FetchRangeObj(void *&start, void *&end, size_t num, size_t size);

    // ��һ���������ڴ�����ͷŵ�Span
    void ReleseListToSpans(void *start, size_t size);

    // ��_spanlists����PageCache��ȡһ��span
    Span *GetOneSpan(size_t size);

    // ��C++11��ʹ�ú����ڵ� local static �������̰߳�ȫ�ģ�����Ҫ����
    // ����ģʽ
    static CentralCache &GetCentralCacheInstance()
    {
        static CentralCache inst;
        return inst;
    }

private:
    CentralCache() = default;

    CentralCache(const CentralCache &) = delete;

    // CentralCache�Ǵ��ھ����ģ����Դ�����ȡ�ڴ��������Ҫ������Ͱ����
    SpanList _spanlists[NFREE_LIST];
};

// ʵ��

Span *CentralCache::GetOneSpan(size_t size)
{
    size_t index = SizeClass::ListIndex(size);
    SpanList &spanlist = _spanlists[index];
    Span *it = spanlist.Begin();
    // spanlist���п�����ڴ����span
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

    // ����Ҫ��PageCache��ȡ��СΪ����ҳ��span
    size_t num_page = SizeClass::NumMovePage(size);

    // ��PageCache ��ȡһ��span
    Span *span = PageCache::GetPageCacheInstance().NewSpan(num_page);

    // ��span�����гɶ�Ӧ��С�ҵ�span��freelist��
    char *start = (char *)(span->m_page_id << PAGE_SHITF);
    char *end = start + (span->m_page_size << PAGE_SHITF);
    while (start < end)
    {
        char *obj = start;
        start += size;

        span->m_span_free_list.Push(obj);
    }
    span->m_obj_size = size;

    // ����ȡ��Span����CentralCache
    spanlist.PushFront(span);

    return span;
}

size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t num, size_t size)
{
    // ���������ڴ�����������CentralCache�е��±�
    size_t index = SizeClass::ListIndex(size);
    SpanList &spanlist = _spanlists[index];
    // ע�����
    spanlist.Lock();
    // ��span�����л�ȡһ��span
    Span *span = GetOneSpan(size);
    FreeList &s_freelist = span->m_span_free_list;
    // PopRange�������������
    size_t actualNum = s_freelist.PopRange(start, end, num);
    span->m_use_count += actualNum;

    spanlist.Unlock();

    return actualNum; // ����ʵ�ʻ�ȡ�����ڴ��������
}

void CentralCache::ReleseListToSpans(void *start, size_t size)
{
    // �����±�
    size_t index = SizeClass::ListIndex(size);
    SpanList &spanlist = _spanlists[index];
    spanlist.Lock();

    // ��next����start����һ���ڴ���󣬲��ƶ�startֱ��ָ���
    while (start)
    {
        void *next = NextObj(start);
        // ��start�ڴ�������ĸ�span
        PAGE_ID id = (PAGE_ID)start >> PAGE_SHITF;                      // ���������ַ����12λ�õ�ҳ��
        Span *span = PageCache::GetPageCacheInstance().GetIdToSpan(id); // ��ҳ�Ų���span
        span->m_span_free_list.Push(start);
        span->m_use_count--;

        // ��ǰspan�г�ȥ�Ķ���ȫ�����أ����Խ�Span����PageCache
        if (span->m_use_count == 0)
        {
            // �õ��±�
            size_t index = SizeClass::ListIndex(span->m_obj_size);
            _spanlists[index].Erase(span);
            span->m_span_free_list.Clear();

            PageCache::GetPageCacheInstance().ReleaseSpanToPageCache(span);
        }

        start = next;
    }

    spanlist.Unlock();
}