cmake_minimum_required(VERSION 3.0)

project(Simd-face)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads)

set(SRCS
    UseFaceDetection.cpp
    )

set(HEADERS
    )

include_directories(../src
    )

add_executable(UseFaceDetection ${SRCS} ${HEADERS})

target_link_libraries(UseFaceDetection ${CMAKE_THREAD_LIBS_INIT} /home/travis/build/s-trinh/Simd/build/libSimd.a)
