#pragma once

#include "../include/ConcurrentMalloc.hpp"
#include <vector>
#include <thread>
#include <iostream>

#define SIZE 16 // ������ڴ��С

/**
 * @brief ����ϵͳmalloc��free
 * @param n_times ÿ�ֵ��ô���
 * @param n_threads �߳���
 * @param n_rounds �ִ�
 */
void BenchmarkMalloc(size_t n_times, size_t n_threads, size_t n_rounds)
{
    cout << "==========================================================" << endl;
    std::vector<std::thread> vthread(n_threads);
    size_t malloc_costtime = 0;
    size_t free_costtime = 0;

    for (size_t k = 0; k < n_threads; ++k)
    {
        vthread[k] = std::thread([&, k]()
                                 {
                                     std::vector<void *> v;
                                     v.reserve(n_times);

                                     for (size_t j = 0; j < n_rounds; ++j)
                                     {
                                         size_t begin1 = clock();
                                         for (size_t i = 0; i < n_times; i++)
                                         {
                                             v.push_back(malloc(SIZE));
                                         }
                                         size_t end1 = clock();

                                         size_t begin2 = clock();
                                         for (size_t i = 0; i < n_times; i++)
                                         {
                                             free(v[i]);
                                         }
                                         size_t end2 = clock();
                                         v.clear();

                                         malloc_costtime += end1 - begin1;
                                         free_costtime += end2 - begin2;
                                     }
                                 });
    }

    for (auto &t : vthread)
    {
        t.join();
    }
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���malloc "
         << n_times << " �Σ���ʱ " << malloc_costtime << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���free "
         << n_times << " �Σ���ʱ " << free_costtime << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���malloc&free "
         << n_times << " �Σ���ʱ " << malloc_costtime + free_costtime << " ms" << endl;
}

/**
 * @brief ConcurrentMalloc
 * @param n_times ÿ�ֵ��ô���
 * @param n_threads �߳���
 * @param n_rounds �ִ�
 */
void BenchmarkConcurrentMalloc(size_t n_times, size_t n_threads, size_t n_rounds)
{
    cout << "==========================================================" << endl;
    std::vector<std::thread> vthread(n_threads);
    size_t malloc_costtime = 0;
    size_t free_costtime = 0;
    for (size_t k = 0; k < n_threads; ++k)
    {
        vthread[k] = std::thread([&]()
                                 {
                                     std::vector<void *> v;
                                     v.reserve(n_times);

                                     for (size_t j = 0; j < n_rounds; ++j)
                                     {
                                         size_t begin1 = clock();
                                         for (size_t i = 0; i < n_times; i++)
                                         {
                                             v.push_back(ConcurrentMalloc(SIZE));
                                         }
                                         size_t end1 = clock();

                                         size_t begin2 = clock();
                                         for (size_t i = 0; i < n_times; i++)
                                         {
                                             ConcurrentFree(v[i]);
                                         }
                                         size_t end2 = clock();
                                         v.clear();

                                         malloc_costtime += end1 - begin1;
                                         free_costtime += end2 - begin2;
                                     }
                                 });
    }

    for (auto &t : vthread)
    {
        t.join();
    }

    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���ConcurrentMalloc "
         << n_times << " �Σ���ʱ " << malloc_costtime << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���free "
         << n_times << " �Σ���ʱ " << free_costtime << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���ConcurrentMalloc & ConcurrentFree "
         << n_times << " �Σ���ʱ " << malloc_costtime + free_costtime << " ms" << endl;
}

/**
 * @brief ���߳�����ռ䣬���ڵ���
 * @param n_times ÿ�ֵ��ô���
 * @param n_rounds �ִ�
 */
void SingleBenchmarkConcurrentMalloc(size_t n_times, size_t n_rounds)
{
    cout << "==========================================================" << endl;

    size_t malloc_costtime = 0;
    size_t free_costtime = 0;

    std::vector<void *> v;
    v.reserve(n_times);
    for (size_t j = 0; j < n_rounds; ++j)
    {
        size_t begin1 = clock();
        for (size_t i = 0; i < n_times; i++)
        {
            v.push_back(ConcurrentMalloc(SIZE));
        }
        size_t end1 = clock();

        size_t begin2 = clock();
        for (size_t i = 0; i < n_times; i++)
        {
            ConcurrentFree(v[i]);
        }
        size_t end2 = clock();
        v.clear();

        malloc_costtime += end1 - begin1;
        free_costtime += end2 - begin2;
    }
    cout << "���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���ConcurrentMalloc "
         << n_times << " �Σ���ʱ " << malloc_costtime << " ms" << endl;
    cout << "���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���free "
         << n_times << " �Σ���ʱ " << free_costtime << " ms" << endl;
    cout << "���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���ConcurrentMalloc & ConcurrentFree "
         << n_times << " �Σ���ʱ " << malloc_costtime + free_costtime << " ms" << endl;
}

int main()
{
    BenchmarkMalloc(10000, 5, 10);
    BenchmarkConcurrentMalloc(10000, 5, 10);
    // SingleBenchmarkConcurrentMalloc(10000, 10);
}