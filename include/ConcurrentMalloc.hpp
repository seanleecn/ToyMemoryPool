/**
 * @file ConcurrentMalloc.hpp
 * @author lixiang (lix0419@outlook.com)
 * @brief �ڴ�ؽӿں���
 */
#pragma once

#include "TreadCache.hpp"

static void *ConcurrentMalloc(size_t size) {
    // [1byte, 64kb] --->��ThreadCache
    if (size <= MAX_SIZE) {
        if (pThreadCache == nullptr) {
            // ÿ���߳�ά��һ��ThreadCache
            pThreadCache = new ThreadCache;
            // cout << std::this_thread::get_id() << "->" << pThreadCache << endl;
        }
        return pThreadCache->Allocate(size);
    }
        // (64kb, 128*4kb] --->��CentralCache
    else if (size <= ((MAX_PAGES - 1) << PAGE_SHITF)) {
        size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHITF);
        size_t pagenum = (align_size >> PAGE_SHITF);
        Span *span = PageCache::GetPageCacheInstance().NewSpan(pagenum);
        span->m_obj_size = align_size;
        void *ptr = (void *) (span->m_page_id << PAGE_SHITF);
        return ptr;
    }
        // [128*4kb,-] --->����128*4kb��ϵͳ����
    else {
        size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHITF);
        size_t pagenum = (align_size >> PAGE_SHITF);
        return SystemAllocate(pagenum);
    }
}

static void ConcurrentFree(void *ptr) {
    size_t pageid = (PAGE_ID) ptr >> PAGE_SHITF;
    Span *span = PageCache::GetPageCacheInstance().GetIdToSpan(pageid);
    // [128*4kb,-]
    if (span == nullptr) {
        SystemFree(ptr);
        return;
    }
    size_t size = span->m_obj_size;
    // [1byte, 64kb]
    if (size <= MAX_SIZE) {
        pThreadCache->deAllocate(ptr, size);
    }
        // (64kb, 128*4kb]
    else if (size <= ((MAX_PAGES - 1) << PAGE_SHITF)) {
        PageCache::GetPageCacheInstance().ReleaseSpanToPageCache(span);
    }
}
