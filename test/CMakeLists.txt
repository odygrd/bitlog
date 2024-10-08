function(bitlog_add_test TEST_NAME SOURCES)
    set(HEADER_FILES
            bundled/doctest.h
    )

    set(ADD_SOURCE_FILES
            doctest_main.cpp)

    list(APPEND SOURCES ${ADD_SOURCE_FILES})

    # Create a test executable
    add_executable(${TEST_NAME} "")

    # Add sources
    target_sources(${TEST_NAME} PRIVATE ${SOURCES} ${HEADER_FILES})

    # include dirs
    target_include_directories(${TEST_NAME}
            PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bundled>
            $<INSTALL_INTERFACE:include>
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})

    # Link dependencies
    target_link_libraries(${TEST_NAME} bitlog)

    # Do not decay cxx standard if not specified
    set_property(TARGET ${TEST_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

    # Set output test directory
    set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/test)

    # Add this target to the post build unit tests
    doctest_discover_tests(${TEST_NAME})
endfunction()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/doctest.cmake)
bitlog_add_test(TEST_BoundedQueue test_bounded_queue.cpp)
bitlog_add_test(TEST_FrontendMarcoMetadata test_frontend_macro_metadata.cpp)
bitlog_add_test(TEST_EncodeDecode test_encode_decode.cpp)
bitlog_add_test(TEST_MetadataFiles test_metadata_files.cpp)
bitlog_add_test(TEST_FrontendSingleton test_frontend_singleton.cpp)
bitlog_add_test(TEST_FrontendImpl test_frontend_impl.cpp)
bitlog_add_test(TEST_ThreadQueueManager test_thread_queue_manager.cpp)
bitlog_add_test(TEST_CachedTimestampBuilder test_cached_timestamp_builder.cpp)
bitlog_add_test(TEST_TimestampFormatter test_timestamp_formatter.cpp)
bitlog_add_test(TEST_PatternFormatter test_pattern_formatter.cpp)
bitlog_add_test(TEST_Log test_log.cpp)