cmake_minimum_required(VERSION 3.0)

project(Simd-face)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(LIBSIMD "" CACHE STRING "LIBSIMD path")

find_package(Threads)

set(SRCS
    UseFaceDetection.cpp
    UseFaceDetection2.cpp
    UseFaceDetection3.cpp
    )

set(HEADERS
    stb_image.h
    )

include_directories(../src
    )

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/haar_face_0.xml DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/1280px-Solvay_conference_1927.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

foreach(cpp ${SRCS})
  get_filename_component(target ${cpp} NAME_WE)
  add_executable(${target} ${cpp} ${HEADERS})
  target_link_libraries(${target} ${CMAKE_THREAD_LIBS_INIT} ${LIBSIMD})
endforeach()
