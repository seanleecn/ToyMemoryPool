#pragma once

#include "../include/ConcurrentMalloc.hpp"
#include <vector>
#include <thread>
#include <iostream>

#define SIZE 16 // 申请的内存大小

/**
 * @brief 调用系统malloc和free
 * @param n_times 每轮调用次数
 * @param n_threads 线程数
 * @param n_rounds 轮次
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
    cout << n_threads << " 个线程并发执行 " << n_rounds << " 轮，每轮调用malloc "
         << n_times << " 次，耗时 " << malloc_costtime << " ms" << endl;
    cout << n_threads << " 个线程并发执行 " << n_rounds << " 轮，每轮调用free "
         << n_times << " 次，耗时 " << free_costtime << " ms" << endl;
    cout << n_threads << " 个线程并发执行 " << n_rounds << " 轮，每轮调用malloc&free "
         << n_times << " 次，耗时 " << malloc_costtime + free_costtime << " ms" << endl;
}

/**
 * @brief ConcurrentMalloc
 * @param n_times 每轮调用次数
 * @param n_threads 线程数
 * @param n_rounds 轮次
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

    cout << n_threads << " 个线程并发执行 " << n_rounds << " 轮，每轮调用ConcurrentMalloc "
         << n_times << " 次，耗时 " << malloc_costtime << " ms" << endl;
    cout << n_threads << " 个线程并发执行 " << n_rounds << " 轮，每轮调用free "
         << n_times << " 次，耗时 " << free_costtime << " ms" << endl;
    cout << n_threads << " 个线程并发执行 " << n_rounds << " 轮，每轮调用ConcurrentMalloc & ConcurrentFree "
         << n_times << " 次，耗时 " << malloc_costtime + free_costtime << " ms" << endl;
}

/**
 * @brief 单线程申请空间，用于调试
 * @param n_times 每轮调用次数
 * @param n_rounds 轮次
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
    cout << "单线程并发执行 " << n_rounds << " 轮，每轮调用ConcurrentMalloc "
         << n_times << " 次，耗时 " << malloc_costtime << " ms" << endl;
    cout << "单线程并发执行 " << n_rounds << " 轮，每轮调用free "
         << n_times << " 次，耗时 " << free_costtime << " ms" << endl;
    cout << "单线程并发执行 " << n_rounds << " 轮，每轮调用ConcurrentMalloc & ConcurrentFree "
         << n_times << " 次，耗时 " << malloc_costtime + free_costtime << " ms" << endl;
}

int main()
{
    BenchmarkMalloc(10000, 5, 10);
    BenchmarkConcurrentMalloc(10000, 5, 10);
    // SingleBenchmarkConcurrentMalloc(10000, 10);
}