cmake_minimum_required(VERSION 3.16)
project(risc201 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

# Shared ISA header. Every module includes isa_spec/isa201.h, so the include
# path is exported through this interface target.
add_library(risc201_isa INTERFACE)
target_include_directories(risc201_isa INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Simulator core: machine state, pipeline and the run loop.
add_library(risc201_core STATIC simulator_core/cpu.cpp)
target_link_libraries(risc201_core PUBLIC risc201_isa)

enable_testing()
add_subdirectory(tests)
