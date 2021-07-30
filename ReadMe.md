# ToyMemoryPool

����TLS(thread local storage)����߼��ڴ��

## �ο�
- Google��[TCMalloc](https://github.com/google/tcmalloc) 
- CSDN���ͣ�[ʵ��һ���߲����ڴ��-----�Ա�Malloc](https://blog.csdn.net/qq_41562665/article/details/90546750)

## ���ʵ��

1. �ڴ�ط�Ϊ����ṹ��ThreadCache��CentralCache��PageCache

   1. ThreadCache���ڷ���С��64kb���ڴ棬��ÿ���߳�ӵ�ж�����TLS���߳����������������Ҳ�Ǳ���Ŀ�Ƚ���malloc���ڶ��̲߳��������µĸ�Ч֮��
   2. CentralCache���뻺���������̵߳��ڴ�������ͷţ���ֹ��Դ������
   3. PageCacheҳ����ͨ������CentralCache�еĿ���span(ʹ�ü���Ϊ0)�����ϲ����ڵ�ҳ�������ڴ���Ƭ�Ĳ�����Central Cacheû��spanʱ����PageCache�����һ���������ڴ�ҳ�����и��һ����С��С���ڴ棬�����Central Cache��

2. ��Ŀ��ʹ�õļ������ݽṹ

   1. span

      �ڴ��ǰ���ҳ���й���ģ�һҳ�ڴ�4kb��span�Ƕ��ҳ�ļ��ϣ�����Ƴ�һ��˫������Ľṹ(��ǰ��ָ��)

   2. FreeList

      

   3. spanList

      

   4. radix

      

3. ����ʹ�õ����ݻ���

   1. ThreadCache

      

   2. CentralCache

      

   3. PageCache

      

4. ����֮��

   1. ��û����ȫ����ϵͳ��malloc���ڽ������ݽṹ����(new Span)��������ڴ�(>512kb)ʱ�������õ�malloc
   2. ֻ����Windows����ʵ��

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

