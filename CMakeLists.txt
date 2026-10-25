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

# Memory safety module
add_library(risc201_memory_safety STATIC
  memory_safety/memory_safety.cpp)
target_link_libraries(risc201_memory_safety PUBLIC risc201_isa)

# Simulator core: machine state, pipeline and the run loop.
add_library(risc201_core STATIC
  simulator_core/cpu.cpp
  alu/alu.cpp
  control_unit/control_unit.cpp
  simulator_core/pipeline.cpp
  simulator_core/cycle_pipeline.cpp
  simulator_core/debugger.cpp
  simulator_core/cli.cpp)
target_link_libraries(risc201_core PUBLIC risc201_isa risc201_memory_safety)

# Command line simulator.
add_executable(risc201_sim simulator_core/main.cpp)
set_target_properties(risc201_sim PROPERTIES OUTPUT_NAME risc201-sim)
target_link_libraries(risc201_sim PRIVATE risc201_core)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Assembler module
add_library(risc201_assembler STATIC assembler/assembler.cpp)
target_link_libraries(risc201_assembler PUBLIC risc201_isa)

add_executable(risc201_asm assembler/main.cpp)
set_target_properties(risc201_asm PROPERTIES OUTPUT_NAME risc201-asm)
target_link_libraries(risc201_asm PRIVATE risc201_assembler)

# Disassembler module
add_library(risc201_disassembler STATIC disassembler/disassembler.cpp)
target_link_libraries(risc201_disassembler PUBLIC risc201_isa)

add_executable(risc201_disasm disassembler/main.cpp)
set_target_properties(risc201_disasm PROPERTIES OUTPUT_NAME risc201-disasm)
target_link_libraries(risc201_disasm PRIVATE risc201_disassembler)

# Evaluation harness: pipeline and microcode comparison numbers.
add_executable(risc201_eval eval/eval_matrix.cpp)
set_target_properties(risc201_eval PROPERTIES OUTPUT_NAME risc201-eval)
target_link_libraries(risc201_eval PRIVATE risc201_core)
target_compile_definitions(risc201_eval
  PRIVATE RISC201_PROGRAMS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/programs")

enable_testing()
add_subdirectory(tests)

# The evaluation run doubles as a smoke test that every program still runs in
# every configuration.
add_test(NAME evaluation COMMAND risc201_eval)
