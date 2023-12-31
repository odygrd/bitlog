cmake_minimum_required(VERSION 3.28)
project(std_module_example CXX)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -march=skylake")

# Create a library
add_library(bitlog)

# Add the module file to the library
target_sources(bitlog
        PUBLIC
        FILE_SET CXX_MODULES FILES
        bitlog.cxx
)

# Create an executable
add_executable(hello main1.cxx)

# Link to the library foo
target_link_libraries(hello bitlog)

# Create an executable
add_executable(hello2 main2.cxx)

# Link to the library foo
target_link_libraries(hello2 bitlog)

add_subdirectory(test)