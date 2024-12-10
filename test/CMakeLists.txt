# test/CMakeLists.txt
add_executable(test_memory test_memory.c)
target_link_libraries(test_memory memory)
target_include_directories(test_memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/memory/include)
