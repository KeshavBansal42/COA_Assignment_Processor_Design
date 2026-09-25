add_executable(test_isa test_isa.cpp)
target_link_libraries(test_isa PRIVATE risc201_isa)
add_test(NAME isa COMMAND test_isa)

add_executable(test_cpu test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE risc201_core)
add_test(NAME cpu COMMAND test_cpu)

add_executable(test_pipeline test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE risc201_core)
add_test(NAME pipeline COMMAND test_pipeline)
