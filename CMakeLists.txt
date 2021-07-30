cmake_minimum_required(VERSION 3.19)
project(ToyMemoryPool)

set(CMAKE_CXX_STANDARD 11)
set(BOOST_ROOT D:/App/boost_1_76_0)
find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(BenchMarkTest
        src/BenchMarkTest.cpp
        include/CentralCache.hpp
        include/Common.hpp
        include/ConcurrentMalloc.hpp
        include/PageCache.hpp
        include/radix.hpp
        include/TreadCache.hpp
        )

add_executable(UnitTest
        src/UnitTest.cpp
        include/CentralCache.hpp
        include/Common.hpp
        include/ConcurrentMalloc.hpp
        include/PageCache.hpp
        include/radix.hpp
        include/TreadCache.hpp
        )
        
