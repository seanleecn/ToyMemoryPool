#include "Common.hpp"
#include "CentralCache.hpp"

/**
 * @brief �߳��ڴ滺�棬ʹ��thread local storage(TLS)��֤�̶߳���
 * ����С��64k���ڴ���䣬���䲻��Ҫ������֤��Ч
 */
class ThreadCache
{
public:
    // �����ڴ�
    void *Allocate(size_t size);

    // �ͷ��ڴ�
    void deAllocate(void *ptr, size_t size);

    // ��CentralCache��ȡ����
    void *FetchFromCentralCache(size_t index);

    // ���freeList����һ�����Ⱦ�Ҫ�ͷŸ�CentralCache
    void ListTooLong(FreeList &freeList, size_t num, size_t size);

private:
    // ά��һ��FreeList���飬ÿ�� [8k,16k,...,64k]
    // ����ͨ�������±��������Ӧ�Ĵ�С�ڴ�
    FreeList _freeLists[NFREE_LIST];
};

// ��̬TSL ��ÿ���߳���Ϊ������ռ�
_declspec(thread) static ThreadCache *pThreadCache = nullptr;

//ʵ��

void *ThreadCache::Allocate(size_t size)
{
    // ��������Ҫ���ڴ������ThreadCache�е��±�
    size_t index = SizeClass::ListIndex(size);
    FreeList &freeList = _freeLists[index];
    // freeList��Ϊ��ֱ�ӷ���
    if (!freeList.Empty())
    {
        return freeList.Pop();
    }
    // �����CentralCache�л�ȡ�ڴ����
    else
    {
        return FetchFromCentralCache(SizeClass::RoundUp(size));
    }
}

void ThreadCache::deAllocate(void *ptr, size_t size)
{
    // �����ͷŵ��ڴ������ThreadCache�е��±�
    size_t index = SizeClass::ListIndex(size);
    FreeList &freeList = _freeLists[index];
    // ���ͷŵ��ڴ������뵽������
    freeList.Push(ptr);

    // ��������е�����
    size_t num = SizeClass::NumMoveSize(size);
    if (freeList.Num() >= num)
    {
        // �������е�ȫ���ڴ�����ͷŻ�CentralCache��
        ListTooLong(freeList, num, size);
    }
}

void ThreadCache::ListTooLong(FreeList &freeList, size_t num, size_t size)
{
    void *start = nullptr, *end = nullptr;
    freeList.PopRange(start, end, num);

    NextObj(end) = nullptr;
    // ReleseListToSpans�������ͷŻ�CentralCache��span��
    CentralCache::GetCentralCacheInstance().ReleseListToSpans(start, size);
}

// �����Ļ����ȡspan
void *ThreadCache::FetchFromCentralCache(size_t size)
{
    size_t num = SizeClass::NumMoveSize(size);

    void *start = nullptr, *end = nullptr;

    // ʵ�ʴ�CentralCache�л�ȡ�˼�������
    // start end�Ƿ��ز���
    size_t actualNum = CentralCache::GetCentralCacheInstance().FetchRangeObj(start, end, num, size);

    // ����ȡ���Ķ���ڴ������ӵ������У������ص�һ���ڴ����
    if (actualNum == 1)
    {
        return start;
    }
    else
    {
        size_t index = SizeClass::ListIndex(size);
        FreeList &list = _freeLists[index];
        list.PushRange(NextObj(start), end, actualNum - 1);
        return start;
    }
}
