#include "alu/alu.hpp"
#include <cassert>
#include <iostream>

void test_addition(risc201::Alu& alu) {
    risc201::AluResult res = alu.execute(risc201::AluOp::ADD, 0xFFFFFFFF, 1);
    
    assert(res.flags.z == true);
    assert(res.flags.c == true);
    std::cout << "Addition test passed!\n";
}

void test_division_by_zero(risc201::Alu& alu) {
    risc201::AluResult res = alu.execute(risc201::AluOp::DIV, 10, 0);
    
    assert(res.exception == risc201::ExceptionCode::DIV_BY_ZERO);
    std::cout << "Division by zero test passed!\n";
}

void test_bitwise(risc201::Alu& alu) {
    risc201::AluResult res = alu.execute(risc201::AluOp::XOR, 0x0F0F0F0F, 0x10101010);
    
    assert(res.flags.n == false);
    std::cout << "Bitwise XOR test passed!\n";
}

int main() {
    risc201::Alu alu; // Standard direct instantiation
    
    std::cout << "Running ALU tests...\n";
    test_addition(alu);
    test_division_by_zero(alu);
    test_bitwise(alu);
    
    std::cout << "All tests passed successfully!\n";
    return 0;
}