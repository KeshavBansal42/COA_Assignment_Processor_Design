add_executable(test_isa test_isa.cpp)
target_link_libraries(test_isa PRIVATE risc201_isa)
add_test(NAME isa COMMAND test_isa)

add_executable(test_alu test_alu.cpp)
target_link_libraries(test_alu PRIVATE risc201_alu)
add_test(NAME alu COMMAND test_alu)

add_executable(test_float32 test_float32.cpp)
target_link_libraries(test_float32 PRIVATE risc201_alu)
# The host arithmetic oracle must honor dynamic rounding and avoid contraction.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_float32 PRIVATE -frounding-math -fno-fast-math -ffp-contract=off)
elseif(MSVC)
  target_compile_options(test_float32 PRIVATE /fp:strict)
endif()
add_test(NAME float32 COMMAND test_float32)

add_executable(test_memory_checks test_memory_checks.cpp)
target_link_libraries(test_memory_checks PRIVATE risc201_memory_safety)
add_test(NAME memory_checks COMMAND test_memory_checks)

add_executable(test_control_unit test_control_unit.cpp)
target_link_libraries(test_control_unit PRIVATE risc201_core)
add_test(NAME control_unit COMMAND test_control_unit)

add_executable(test_control_integration test_control_integration.cpp)
target_link_libraries(test_control_integration PRIVATE risc201_core)
add_test(NAME control_integration COMMAND test_control_integration)

add_executable(test_microcode test_microcode.cpp)
target_link_libraries(test_microcode PRIVATE risc201_core)
add_test(NAME microcode COMMAND test_microcode)

add_executable(test_cpu test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE risc201_core)
add_test(NAME cpu COMMAND test_cpu)

add_executable(test_pipeline test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE risc201_core)
add_test(NAME pipeline COMMAND test_pipeline)

add_executable(test_pipeline_hazards test_pipeline_hazards.cpp)
target_link_libraries(test_pipeline_hazards PRIVATE risc201_core)
target_compile_definitions(test_pipeline_hazards
  PRIVATE RISC201_PROGRAMS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/programs")
add_test(NAME pipeline_hazards COMMAND test_pipeline_hazards)

add_executable(test_full_isa test_full_isa.cpp)
target_link_libraries(test_full_isa PRIVATE risc201_core)
add_test(NAME full_isa COMMAND test_full_isa)

add_executable(test_exceptions test_exceptions.cpp)
target_link_libraries(test_exceptions PRIVATE risc201_core)
add_test(NAME exceptions COMMAND test_exceptions)

add_executable(test_cli test_cli.cpp)
target_link_libraries(test_cli PRIVATE risc201_core)
add_test(NAME cli COMMAND test_cli)

add_executable(test_debugger test_debugger.cpp)
target_link_libraries(test_debugger PRIVATE risc201_core)
add_test(NAME debugger COMMAND test_debugger)

# Runs the demo programs in tests/programs through the CLI.
add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE risc201_core)
target_compile_definitions(test_integration
  PRIVATE RISC201_PROGRAMS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/programs")
add_test(NAME integration COMMAND test_integration)

add_executable(test_assembler test_assembler.cpp)
target_link_libraries(test_assembler PRIVATE risc201_assembler)
add_test(NAME assembler COMMAND test_assembler)

add_executable(test_disassembler test_disassembler.cpp)
target_link_libraries(test_disassembler PRIVATE risc201_disassembler risc201_assembler)
add_test(NAME disassembler COMMAND test_disassembler)

add_executable(test_visualizer test_visualizer.cpp)
target_link_libraries(test_visualizer PRIVATE risc201_visualizer)
add_test(NAME visualizer COMMAND test_visualizer)


add_executable(test_float test_float.cpp)
target_link_libraries(test_float PRIVATE risc201_core risc201_assembler risc201_disassembler)
add_test(NAME float COMMAND test_float)

add_executable(test_branch_predictor test_branch_predictor.cpp)
target_link_libraries(test_branch_predictor PRIVATE risc201_core)
target_compile_definitions(test_branch_predictor
  PRIVATE RISC201_PROGRAMS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/programs")
add_test(NAME branch_predictor COMMAND test_branch_predictor)
