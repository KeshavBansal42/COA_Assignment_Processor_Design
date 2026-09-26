add_executable(test_isa test_isa.cpp)
target_link_libraries(test_isa PRIVATE risc201_isa)
add_test(NAME isa COMMAND test_isa)

add_executable(test_cpu test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE risc201_core)
add_test(NAME cpu COMMAND test_cpu)

add_executable(test_pipeline test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE risc201_core)
add_test(NAME pipeline COMMAND test_pipeline)

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
