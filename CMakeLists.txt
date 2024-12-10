cmake_minimum_required(VERSION 3.10)
project(MemoryAllocator)

# Añadir subdirectorios
add_subdirectory(lib/memory)

# Añadir el ejecutable de prueba
add_executable(main main.c)
target_link_libraries(main memory)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/memory/include)
