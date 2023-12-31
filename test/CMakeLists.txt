# Create an executable
add_executable(bounded_queue_test bounded_queue_test.cpp doctest_main.cpp)

# Link to the library foo
target_link_libraries(bounded_queue_test bitlog)