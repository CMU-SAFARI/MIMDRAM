cmake_minimum_required(VERSION 3.1)
project(PIMVectorizationPass)

# support C++14 features used by LLVM 10.0.0
set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} include)
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(src)
