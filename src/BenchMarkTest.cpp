#pragma once

#include "../include/ConcurrentMalloc.hpp"
#include <vector>
#include <thread>
#include <iostream>

const static int bytes = 70000;    // ������ڴ��С
const static int threads = 5;   // �����߳�
const static int rounds = 10;   // �ִ�
const static int times = 10000; // ÿ���߳����뼸��

void BenchmarkMalloc(size_t n_times, size_t n_threads, size_t n_rounds) {
    std::vector<std::thread> thread_vec(n_threads);
    size_t malloc_cost_time = 0;
    size_t free_cost_time = 0;
    for (size_t k = 0; k < n_threads; ++k) {
        thread_vec[k] = std::thread([&]() {
            std::vector<void *> v;
            v.reserve(n_times);

            for (size_t j = 0; j < n_rounds; ++j) {
                size_t begin1 = clock();
                for (size_t i = 0; i < n_times; i++) {
                    v.push_back(malloc(bytes));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for (size_t i = 0; i < n_times; i++) {
                    free(v[i]);
                }
                size_t end2 = clock();
                v.clear();

                malloc_cost_time += end1 - begin1;
                free_cost_time += end2 - begin2;
            }
        });
    }
    for (auto &t : thread_vec) {
        t.join();
    }
    cout << "==========================================================" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���malloc "
         << n_times << " �Σ���ʱ " << malloc_cost_time << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���free "
         << n_times << " �Σ���ʱ " << free_cost_time << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���malloc&free "
         << n_times << " �Σ���ʱ " << malloc_cost_time + free_cost_time << " ms" << endl;
}

void BenchmarkConcurrentMalloc(size_t n_times, size_t n_threads, size_t n_rounds) {
    std::vector<std::thread> thread_vec(n_threads);
    size_t malloc_cost_time = 0;
    size_t free_cost_time = 0;
    for (size_t k = 0; k < n_threads; ++k) {
        thread_vec[k] = std::thread([&]() {
            std::vector<void *> v;
            v.reserve(n_times);

            for (size_t j = 0; j < n_rounds; ++j) {
                size_t begin1 = clock();
                for (size_t i = 0; i < n_times; i++) {
                    v.push_back(ConcurrentMalloc(bytes));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for (size_t i = 0; i < n_times; i++) {
                    ConcurrentFree(v[i]);
                }
                size_t end2 = clock();
                v.clear();

                malloc_cost_time += end1 - begin1;
                free_cost_time += end2 - begin2;
            }
        });
    }

    for (auto &t : thread_vec) {
        t.join();
    }
    cout << "==========================================================" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���ConcurrentMalloc "
         << n_times << " �Σ���ʱ " << malloc_cost_time << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���free "
         << n_times << " �Σ���ʱ " << free_cost_time << " ms" << endl;
    cout << n_threads << " ���̲߳���ִ�� " << n_rounds << " �֣�ÿ�ֵ���ConcurrentMalloc & ConcurrentFree "
         << n_times << " �Σ���ʱ " << malloc_cost_time + free_cost_time << " ms" << endl;
}

int main() {
    BenchmarkMalloc(times, threads, rounds);
    BenchmarkConcurrentMalloc(times, threads, rounds);
}