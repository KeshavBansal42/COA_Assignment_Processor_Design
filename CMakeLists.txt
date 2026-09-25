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
add_library(risc201_core STATIC
  simulator_core/cpu.cpp
  simulator_core/pipeline.cpp
  simulator_core/hardwired_control.cpp
  simulator_core/simple_alu.cpp
  simulator_core/simple_safety.cpp
  simulator_core/debugger.cpp
  simulator_core/cli.cpp)
target_link_libraries(risc201_core PUBLIC risc201_isa)

# Command line simulator.
add_executable(risc201_sim simulator_core/main.cpp)
set_target_properties(risc201_sim PROPERTIES OUTPUT_NAME risc201-sim)
target_link_libraries(risc201_sim PRIVATE risc201_core)

enable_testing()
add_subdirectory(tests)
