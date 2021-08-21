#include "../include/ConcurrentMalloc.hpp"
#include <vector>

void UnitRoundUpTest() {
    cout << SizeClass::RoundUp(1) << endl;
    cout << SizeClass::RoundUp(128) << endl;

    vector<int> round_vec;
    for (int i = 1; i <= 65536; i++) {
        int roundup = SizeClass::RoundUp(i);
        if (!round_vec.empty() && roundup == round_vec.back())
            continue;
        round_vec.push_back(roundup);
    }
    cout << "64kb以内内存被向上取整为：" << round_vec.size() << "种结果" << endl;
    for (int &i : round_vec)
        cout << i << " ";
    cout << endl;
}

void UnitListIndexTest() {
    cout << SizeClass::ListIndex(1024*8+1) << endl;
    cout << SizeClass::ListIndex(64*1024) << endl;

//    vector<int> idx_vec;
//    for (int i = 1; i <= 65536; i++) {
//        int idx = SizeClass::ListIndex(i);
//        if (!idx_vec.empty() && idx == idx_vec.back())
//            continue;
//        idx_vec.push_back(idx);
//    }
//    cout << "一共有：" << idx_vec.size() << "种链表" << endl;
//    for (int &i : idx_vec)
//        cout << i << " ";
//    cout << endl;
}

void UnitTestSystemAlloc() {
    void *ptr1 = SystemAllocate(1);
    void *ptr2 = SystemAllocate(1);

    PAGE_ID id1 = (PAGE_ID) ptr1 >> PAGE_SHITF;
    PAGE_ID id2 = (PAGE_ID) ptr2 >> PAGE_SHITF;

    void *ptrshift1 = (void *) (id1 << PAGE_SHITF);
    void *ptrshift2 = (void *) (id2 << PAGE_SHITF);

    char *obj1 = (char *) ptr1;
    char *obj2 = (char *) ptr1 + 8;
    char *obj3 = (char *) ptr1 + 16;

    PAGE_ID idd1 = (PAGE_ID) obj1 >> PAGE_SHITF;
    PAGE_ID idd2 = (PAGE_ID) obj2 >> PAGE_SHITF;
    PAGE_ID idd3 = (PAGE_ID) obj3 >> PAGE_SHITF;
}

void func() {
    std::vector<void *> v;
    size_t size = 7;
    for (size_t i = 0; i < 512; ++i) {
        v.push_back(ConcurrentMalloc(size));
    }
    v.push_back(ConcurrentMalloc(size));

    for (size_t i = 0; i < v.size(); ++i) {
    }

    for (auto ptr : v) {
        ConcurrentFree(ptr);
    }
    v.clear();
}

void UnitTestPAGE_SHITF() {
    void *ptr1 = ConcurrentMalloc(1 << PAGE_SHITF);
    void *ptr2 = ConcurrentMalloc(65 << PAGE_SHITF);
    void *ptr3 = ConcurrentMalloc(129 << PAGE_SHITF);

    ConcurrentFree(ptr1);
    ConcurrentFree(ptr2);
    ConcurrentFree(ptr3);
}

int main() {
    UnitRoundUpTest();
//    UnitListIndexTest();
//    UnitTestSystemAlloc();
//    UnitTestConcurrentMalloc();
//    UnitTestPAGE_SHITF();
//    func();
}