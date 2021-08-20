# ToyMemoryPool

基于TLS(thread local storage)的玩具级内存池。适用于多线程情况下使用。

## 参考

- Google [TCMalloc](https://github.com/google/tcmalloc) 
- 知乎：[图解 TCMalloc](https://zhuanlan.zhihu.com/p/29216091)
- CSDN博客：[实现一个高并发内存池-----对比Malloc](https://blog.csdn.net/qq_41562665/article/details/90546750)

## 特点

- 高并发

  每一个线程都有着自己的一个线程缓存，当每一个线程申请内存小于64kb的时候就不需要每次要到系统申请内存直接到自己的线程缓存上申请内存就好了。就不会牵扯到多个线程访问同一份资源加锁的问题，就达到了高并发的目的。

- 内存碎片

  对于**外碎片**进行了减少，线程缓存不使用的内存就会归还到中心缓存的一个Span上，而中心缓存的Span上的内存只要没有线程使用的话，就将这个span再次归还到页缓存上，归还到页缓存的时候就会对多个span进行合并，从而将小的内存合并成大的内存。

## 数据结构

#### FreeList

链表结构，每个元素是一定大小的内存对象。

ThreadCache中用FreeList**数组**管理内存，实际测试共184个链表

```
// 对齐大小计算，向上取整，设置了四个对齐数
// 申请内存byte          对齐数         对应链表数组下标
// [1, 128]             8byte对齐     FreeList[0,15)
// [129, 1024]          16byte对齐    FreeList[16,72)
// [1025, 8*1024]       128byte对齐   FreeList[72,128)
// [8*1024+1, 64*1024]  1024byte对齐  FreeList[128,184)
```

#### Span

内存是按照页进行管理的，一页内存4kb，Span是多个连续页的集合，并带有前后指针。

```
struct Span {
    PAGE_ID m_page_id = 0;     // 页号，因为内存是按页分配的，一页为4kb，内存地址/4kb得到唯一的值页号，且同一页上的所有内存的页号都是一样的
    PAGE_ID m_page_size = 0;   // 当前Span维护的页的数量
    size_t m_obj_size = 0;     // 将该Span按多大内存对象进行分割，centralcache中使用到
    FreeList m_span_free_list; // 将Span里分割后的内存对象用FreeList链表连接起来
    int m_use_count = 0;       // 内存对象被使用的数量。当值为0，说明没有被使用，此时把内存返回PageCache

    Span *_next = nullptr;
    Span *_prev = nullptr;
};
```

#### SpanList

Span的链表形式。

CentralCache和PageCache中用SpanList**数组**管理内存。

CentralCache有184个SpanList，每个SpanList中的内存拆成小块用FreeList的形式连起来。第一个SpanList中的Span下面挂的是8bytes的内存，第二层的Span挂的是16bytes的内存，参考对齐数；

PageCache有129个SpanList，SpanList中的Span不切分成更小的内存。第一个SpanList中的Span保存一个页框，第二个SpanList中的Span保存两个页框，以此类推。最多可以管理128*4kb = 512kb的内存。

#### radix 基数树

TODO

目前使用的是boost实现好的结构，根据维基百科介绍，基数树更适用于对于较小的集合（尤其是字符串很长的情况下）和有很长相同前缀的字符串集合。盲猜这和内存地址很像，前缀相同。

就当成一个哈希表来用，用于根据页号查询Span

## 结构

内存池分为三层结构：ThreadCache、CentralCache、PageCache 

### ThreadCache

用于分配小于64kb的内存，且每个线程拥有独立的TLS，线程申请无需加锁，这也是本项目比较与malloc的在多线程并发条件下的高效之处。

``` C++
_declspec(thread)
static ThreadCache *pThreadCache = nullptr;
```

> ``_declspec(thread)``是Windows下面的接口，参考[msvc](https://docs.microsoft.com/zh-cn/cpp/cpp/declspec?view=msvc-160)
> 
>其他申请线程局部变量的方法：GCC的``__thread``，C++11的[thread_local](https://murphypei.github.io/blog/2020/02/thread-local)

#### 申请：

1. 申请内存小于64kb的时候，先根据对齐数向上取整，然后从链表数组中找到对应的FreeList，如果这个FreeList中有内存对象，就pop出来返回
2. 当FreeList中没有内存对象了，就像CentralCache申请

#### 释放：

1. 计算释放内存对象在数组中的下标，插入这个FreeList的链表中
2. 检查当前FreeList长度是否过长（链表上面挂的内存综合大于64kb），过长要将一部分内存对象返回给CentralCache

### CentralCache

中央缓存设计为**单例模式**，均衡各个线程的内存申请和释放，防止资源饥饿。

中心缓存向各个线程的ThreadCache分配内存，并且从中回收内存，防止单个线程占用过多内存。

当一个线程大量的开辟内存再释放的时候，这个线程中的ThreadCache会储存着大量的空闲内存资源，而这些资源是无法被其他线程所使用的，当其他的线程需要使用内存资源时，就可能没有更多的内存资源可以使用，这也就导致了其它线程的饥饿问题。

CentralCache的访问需要**加锁**，但是由于每次都会从CentralCache获取很多内存，因此这里的锁竞争并不频繁。

#### 申请：

1. 计算申请内存在SpanList数组的下标，取到一个SpanList，遍历这个链表上面的Span的将m_span_free_list不是空的返回，并++m_use_count表示这个Span被一个线程使用了

#### 释放：

1. 从ThreadCache中返回的内存计算其m_page_id，这里要注意将返回的内存对象返回到原来的Span
2. 通过radix查找到其Span对象
3. 将返回的内存对象插入到Span对象中的m_span_free_list上
4. 判断此时use_count是否为0，为0则将Span对象返还给PageCache

### PageCache

向系统申请内存，并且按页的大小进行划分。

页缓存通过回收CentralCache中的空闲Span(使用计数为0)，并合并相邻的页，缓解内存碎片的产生。

CentralCache没有Span时，从PageCache分配出一定数量的内存页，并切割成一定大小的小块内存，分配给CentralCache。

#### 申请：

1. CentralCache向PageCache申请获取Span
2. PageCache根据要获取的Span对象中页的大小计算出所在数组下标。
3. 如果该数组对应的Span链表有Span对象，则直接返回。
4. 如果没有，则要进行切页。
   1. 通过从小到大遍历数组，找到一个比当前需要的页大的Span对象。
   2. 将其切分成两个Span对象，一个满足需要页大小的Span对象，一个是剩余页构成的Span对象。
   3. 将得到的两个新的Span对象插入到Radix中，更新这个Radix映射
   4. 返回切页后满足条件页大小的Span对象

#### 释放：

1. CentralCache返回一个Span对象
2. **伙伴算法**的合并技术。根据Span对象的m_page_id计算其前一页的m_page_id，查看前面页对应的Span对象是否没有被使用，如果没有就将两个Span对象合并，这是**向前合并**。同样的方法找到后面一页对应的Span向后合并。这样小的页的Span就合并成大的页的Span。
3. 更新redix映射
4. 如果当前Span包含了超过128页就将其返回给系统。

## 接口

> 下面的函数流程图按照内存池的三层结构分成不同的颜色分区，红色表示ThreadCache中的操作，绿色表示CentralCache中的操作，蓝色表示PageCache中的操作。

- ConcurrentMalloc

  用于申请内存，详细流程

![ConcurrentMalloc](./pic/ConcurrentMalloc.png)

- ConcurrentFree

  用于释放内存，详细流程

![ConcurrentFree](./pic/ConcurrentFree.png)

## 测试

![test](./pic/test.png)

## 不足之处

1. 并没有完全脱离系统的malloc，在建立数据结构对象(new Span)和申请大内存(>512kb)时，还是用到malloc
2. 只是在Windows下面实现