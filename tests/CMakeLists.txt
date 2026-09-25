add_executable(test_isa test_isa.cpp)
target_link_libraries(test_isa PRIVATE risc201_isa)
add_test(NAME isa COMMAND test_isa)
