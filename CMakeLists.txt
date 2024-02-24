cmake_minimum_required(VERSION 3.27)
project(bitlog CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64 -mavx2 -mavx -msse3 -msse2 -msse")

# Create a library
add_library(bitlog INTERFACE)

target_include_directories(bitlog INTERFACE include)

# Create an executable
add_executable(hello main1.cpp)

# Link to the library foo
target_link_libraries(hello bitlog)

## Create an executable
#add_executable(hello2 main2.cxx)
#
## Link to the library foo
#target_link_libraries(hello2 bitlog)
#
add_subdirectory(test)