# ToyMemoryPool

����TLS(thread local storage)����߼��ڴ�ء������ڶ��߳������ʹ�á�

## �ο�

- Google [TCMalloc](https://github.com/google/tcmalloc) 
- ֪����[ͼ�� TCMalloc](https://zhuanlan.zhihu.com/p/29216091)
- CSDN���ͣ�[ʵ��һ���߲����ڴ��-----�Ա�Malloc](https://blog.csdn.net/qq_41562665/article/details/90546750)

## �ص�

- �߲���

  ÿһ���̶߳������Լ���һ���̻߳��棬��ÿһ���߳������ڴ�С��64kb��ʱ��Ͳ���Ҫÿ��Ҫ��ϵͳ�����ڴ�ֱ�ӵ��Լ����̻߳����������ڴ�ͺ��ˡ��Ͳ���ǣ��������̷߳���ͬһ����Դ���������⣬�ʹﵽ�˸߲�����Ŀ�ġ�

- �ڴ���Ƭ

  ����**����Ƭ**�����˼��٣��̻߳��治ʹ�õ��ڴ�ͻ�黹�����Ļ����һ��Span�ϣ������Ļ����Span�ϵ��ڴ�ֻҪû���߳�ʹ�õĻ����ͽ����span�ٴι黹��ҳ�����ϣ��黹��ҳ�����ʱ��ͻ�Զ��span���кϲ����Ӷ���С���ڴ�ϲ��ɴ���ڴ档

## ���ݽṹ

#### FreeList

����ṹ��ÿ��Ԫ����һ����С���ڴ����

ThreadCache����FreeList**����**�����ڴ棬ʵ�ʲ��Թ�184������

```
// �����С���㣬����ȡ�����������ĸ�������
// �����ڴ�byte          ������         ��Ӧ���������±�
// [1, 128]             8byte����     FreeList[0,15)
// [129, 1024]          16byte����    FreeList[16,72)
// [1025, 8*1024]       128byte����   FreeList[72,128)
// [8*1024+1, 64*1024]  1024byte����  FreeList[128,184)
```

#### Span

�ڴ��ǰ���ҳ���й���ģ�һҳ�ڴ�4kb��Span�Ƕ������ҳ�ļ��ϣ�������ǰ��ָ�롣

```
struct Span {
    PAGE_ID m_page_id = 0;     // ҳ�ţ���Ϊ�ڴ��ǰ�ҳ����ģ�һҳΪ4kb���ڴ��ַ/4kb�õ�Ψһ��ֵҳ�ţ���ͬһҳ�ϵ������ڴ��ҳ�Ŷ���һ����
    PAGE_ID m_page_size = 0;   // ��ǰSpanά����ҳ������
    size_t m_obj_size = 0;     // ����Span������ڴ������зָcentralcache��ʹ�õ�
    FreeList m_span_free_list; // ��Span��ָ����ڴ������FreeList������������
    int m_use_count = 0;       // �ڴ����ʹ�õ���������ֵΪ0��˵��û�б�ʹ�ã���ʱ���ڴ淵��PageCache

    Span *_next = nullptr;
    Span *_prev = nullptr;
};
```

#### SpanList

Span��������ʽ��

CentralCache��PageCache����SpanList**����**�����ڴ档

CentralCache��184��SpanList��ÿ��SpanList�е��ڴ���С����FreeList����ʽ����������һ��SpanList�е�Span����ҵ���8bytes���ڴ棬�ڶ����Span�ҵ���16bytes���ڴ棬�ο���������

PageCache��129��SpanList��SpanList�е�Span���зֳɸ�С���ڴ档��һ��SpanList�е�Span����һ��ҳ�򣬵ڶ���SpanList�е�Span��������ҳ���Դ����ơ������Թ���128*4kb = 512kb���ڴ档

#### radix ������

TODO

Ŀǰʹ�õ���boostʵ�ֺõĽṹ������ά���ٿƽ��ܣ��������������ڶ��ڽ�С�ļ��ϣ��������ַ����ܳ�������£����кܳ���ͬǰ׺���ַ������ϡ�ä������ڴ��ַ����ǰ׺��ͬ��

�͵���һ����ϣ�����ã����ڸ���ҳ�Ų�ѯSpan

## �ṹ

�ڴ�ط�Ϊ����ṹ��ThreadCache��CentralCache��PageCache 

### ThreadCache

���ڷ���С��64kb���ڴ棬��ÿ���߳�ӵ�ж�����TLS���߳����������������Ҳ�Ǳ���Ŀ�Ƚ���malloc���ڶ��̲߳��������µĸ�Ч֮����

``` C++
_declspec(thread)
static ThreadCache *pThreadCache = nullptr;
```

> ``_declspec(thread)``��Windows����Ľӿڣ��ο�[msvc](https://docs.microsoft.com/zh-cn/cpp/cpp/declspec?view=msvc-160)
> 
>���������ֲ߳̾������ķ�����GCC��``__thread``��C++11��[thread_local](https://murphypei.github.io/blog/2020/02/thread-local)

#### ���룺

1. �����ڴ�С��64kb��ʱ���ȸ��ݶ���������ȡ����Ȼ��������������ҵ���Ӧ��FreeList��������FreeList�����ڴ���󣬾�pop��������
2. ��FreeList��û���ڴ�����ˣ�����CentralCache����

#### �ͷţ�

1. �����ͷ��ڴ�����������е��±꣬�������FreeList��������
2. ��鵱ǰFreeList�����Ƿ��������������ҵ��ڴ��ۺϴ���64kb��������Ҫ��һ�����ڴ���󷵻ظ�CentralCache

### CentralCache

���뻺�����Ϊ**����ģʽ**����������̵߳��ڴ�������ͷţ���ֹ��Դ������

���Ļ���������̵߳�ThreadCache�����ڴ棬���Ҵ��л����ڴ棬��ֹ�����߳�ռ�ù����ڴ档

��һ���̴߳����Ŀ����ڴ����ͷŵ�ʱ������߳��е�ThreadCache�ᴢ���Ŵ����Ŀ����ڴ���Դ������Щ��Դ���޷��������߳���ʹ�õģ����������߳���Ҫʹ���ڴ���Դʱ���Ϳ���û�и�����ڴ���Դ����ʹ�ã���Ҳ�͵����������̵߳ļ������⡣

CentralCache�ķ�����Ҫ**����**����������ÿ�ζ����CentralCache��ȡ�ܶ��ڴ棬������������������Ƶ����

#### ���룺

1. ���������ڴ���SpanList������±꣬ȡ��һ��SpanList������������������Span�Ľ�m_span_free_list���ǿյķ��أ���++m_use_count��ʾ���Span��һ���߳�ʹ����

#### �ͷţ�

1. ��ThreadCache�з��ص��ڴ������m_page_id������Ҫע�⽫���ص��ڴ���󷵻ص�ԭ����Span
2. ͨ��radix���ҵ���Span����
3. �����ص��ڴ������뵽Span�����е�m_span_free_list��
4. �жϴ�ʱuse_count�Ƿ�Ϊ0��Ϊ0��Span���󷵻���PageCache

### PageCache

��ϵͳ�����ڴ棬���Ұ�ҳ�Ĵ�С���л��֡�

ҳ����ͨ������CentralCache�еĿ���Span(ʹ�ü���Ϊ0)�����ϲ����ڵ�ҳ�������ڴ���Ƭ�Ĳ�����

CentralCacheû��Spanʱ����PageCache�����һ���������ڴ�ҳ�����и��һ����С��С���ڴ棬�����CentralCache��

#### ���룺

1. CentralCache��PageCache�����ȡSpan
2. PageCache����Ҫ��ȡ��Span������ҳ�Ĵ�С��������������±ꡣ
3. ����������Ӧ��Span������Span������ֱ�ӷ��ء�
4. ���û�У���Ҫ������ҳ��
   1. ͨ����С����������飬�ҵ�һ���ȵ�ǰ��Ҫ��ҳ���Span����
   2. �����зֳ�����Span����һ��������Ҫҳ��С��Span����һ����ʣ��ҳ���ɵ�Span����
   3. ���õ��������µ�Span������뵽Radix�У��������Radixӳ��
   4. ������ҳ����������ҳ��С��Span����

#### �ͷţ�

1. CentralCache����һ��Span����
2. **����㷨**�ĺϲ�����������Span�����m_page_id������ǰһҳ��m_page_id���鿴ǰ��ҳ��Ӧ��Span�����Ƿ�û�б�ʹ�ã����û�оͽ�����Span����ϲ�������**��ǰ�ϲ�**��ͬ���ķ����ҵ�����һҳ��Ӧ��Span���ϲ�������С��ҳ��Span�ͺϲ��ɴ��ҳ��Span��
3. ����redixӳ��
4. �����ǰSpan�����˳���128ҳ�ͽ��䷵�ظ�ϵͳ��

## �ӿ�

> ����ĺ�������ͼ�����ڴ�ص�����ṹ�ֳɲ�ͬ����ɫ��������ɫ��ʾThreadCache�еĲ�������ɫ��ʾCentralCache�еĲ�������ɫ��ʾPageCache�еĲ�����

- ConcurrentMalloc

  ���������ڴ棬��ϸ����

![ConcurrentMalloc](./pic/ConcurrentMalloc.png)

- ConcurrentFree

  �����ͷ��ڴ棬��ϸ����

![ConcurrentFree](./pic/ConcurrentFree.png)

## ����

![test](./pic/test.png)

## ����֮��

1. ��û����ȫ����ϵͳ��malloc���ڽ������ݽṹ����(new Span)��������ڴ�(>512kb)ʱ�������õ�malloc
2. ֻ����Windows����ʵ��