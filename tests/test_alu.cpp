#include "alu/alu.hpp"
#include <cassert>
#include <iostream>

void test_addition(risc201::Alu& alu) {
    // Normal add
    risc201::AluResult r1 = alu.execute(risc201::AluOp::ADD, 10, 20);
    assert(r1.value == 30);
    assert(!r1.flags.z && !r1.flags.n && !r1.flags.c && !r1.flags.v);

    // Carry out without signed overflow: 0xFFFFFFFF + 1 = 0
    risc201::AluResult r2 = alu.execute(risc201::AluOp::ADD, 0xFFFFFFFF, 1);
    assert(r2.value == 0);
    assert(r2.flags.z == true);
    assert(r2.flags.c == true);
    assert(r2.flags.v == false);
    assert(r2.flags.n == false);

    // Signed overflow: 0x7FFFFFFF (+MAX) + 1 = 0x80000000 (-MIN)
    risc201::AluResult r3 = alu.execute(risc201::AluOp::ADD, 0x7FFFFFFF, 1);
    assert(r3.value == 0x80000000);
    assert(r3.flags.v == true);
    assert(r3.flags.n == true);
    assert(r3.flags.c == false);

    std::cout << "[PASS] CLA Addition & Flags tests\n";
}

void test_subtraction_and_cmp(risc201::Alu& alu) {
    // Normal sub: 20 - 10 = 10 (c=1: no borrow, a >= b)
    risc201::AluResult r1 = alu.execute(risc201::AluOp::SUB, 20, 10);
    assert(r1.value == 10);
    assert(!r1.flags.z && !r1.flags.n && r1.flags.c && !r1.flags.v);

    // Sub producing negative: 10 - 20 = -10 (c=0: borrow, a < b)
    risc201::AluResult r2 = alu.execute(risc201::AluOp::SUB, 10, 20);
    assert(r2.value == static_cast<uint32_t>(-10));
    assert(r2.flags.n == true);
    assert(r2.flags.c == false);
    assert(r2.flags.v == false);

    // Sub zero result: 15 - 15 = 0
    risc201::AluResult r3 = alu.execute(risc201::AluOp::SUB, 15, 15);
    assert(r3.value == 0);
    assert(r3.flags.z == true);
    assert(r3.flags.c == true);

    // Signed overflow on subtraction: 0x80000000 - 1 = 0x7FFFFFFF
    risc201::AluResult r4 = alu.execute(risc201::AluOp::SUB, 0x80000000, 1);
    assert(r4.value == 0x7FFFFFFF);
    assert(r4.flags.v == true);
    assert(r4.flags.n == false);

    // CMP sets flags identical to SUB
    risc201::AluResult r5 = alu.execute(risc201::AluOp::CMP, 42, 42);
    assert(r5.flags.z == true);
    assert(r5.flags.c == true);

    std::cout << "[PASS] CLA Subtraction & CMP tests\n";
}

void test_multiplication(risc201::Alu& alu) {
    // Positive * Positive
    risc201::AluResult r1 = alu.execute(risc201::AluOp::MUL, 6, 7);
    assert(r1.value == 42);
    assert(!r1.flags.v && !r1.flags.n && !r1.flags.z);

    // Positive * Negative: 5 * (-4) = -20
    risc201::AluResult r2 = alu.execute(risc201::AluOp::MUL, 5, static_cast<uint32_t>(-4));
    assert(r2.value == static_cast<uint32_t>(-20));
    assert(r2.flags.n == true);
    assert(r2.flags.v == false);

    // Negative * Negative: (-3) * (-4) = 12
    risc201::AluResult r3 = alu.execute(risc201::AluOp::MUL, static_cast<uint32_t>(-3), static_cast<uint32_t>(-4));
    assert(r3.value == 12);
    assert(!r3.flags.n && !r3.flags.v);

    // Multiply by zero
    risc201::AluResult r4 = alu.execute(risc201::AluOp::MUL, 0, 9999);
    assert(r4.value == 0);
    assert(r4.flags.z == true);

    // Signed overflow: 0x7FFFFFFF * 2
    risc201::AluResult r5 = alu.execute(risc201::AluOp::MUL, 0x7FFFFFFF, 2);
    assert(r5.flags.v == true);

    std::cout << "[PASS] Booth's Algorithm Multiplication tests\n";
}

void test_division(risc201::Alu& alu) {
    // Division by zero exception
    risc201::AluResult r_zero = alu.execute(risc201::AluOp::DIV, 10, 0);
    assert(r_zero.exception == risc201::ExceptionCode::DIV_BY_ZERO);

    // Standard unsigned division: 100 / 7 = 14
    risc201::AluResult r1 = alu.execute(risc201::AluOp::DIV, 100, 7);
    assert(r1.value == 14);
    assert(r1.exception == risc201::ExceptionCode::NONE);

    // A < B gives 0: 5 / 10 = 0
    risc201::AluResult r2 = alu.execute(risc201::AluOp::DIV, 5, 10);
    assert(r2.value == 0);
    assert(r2.flags.z == true);

    // Large unsigned numbers (> 2^31) testing 33-bit restoring accumulator:
    // 3,000,000,000 / 2 = 1,500,000,000
    risc201::AluResult r3 = alu.execute(risc201::AluOp::DIV, 3000000000u, 2u);
    assert(r3.value == 1500000000u);

    // 0xFFFFFFFF / 2 = 0x7FFFFFFF
    risc201::AluResult r4 = alu.execute(risc201::AluOp::DIV, 0xFFFFFFFFu, 2u);
    assert(r4.value == 0x7FFFFFFFu);

    // 0xFFFFFFFF / 0xFFFFFFFF = 1
    risc201::AluResult r5 = alu.execute(risc201::AluOp::DIV, 0xFFFFFFFFu, 0xFFFFFFFFu);
    assert(r5.value == 1u);

    // Deterministic C++ stress test: 10,000 pseudo-random divisions
    uint32_t seed = 123456789;
    for (int i = 0; i < 10000; i++) {
        seed = seed * 1664525u + 1013904223u;
        uint32_t a = seed;
        seed = seed * 1664525u + 1013904223u;
        uint32_t b = (seed == 0) ? 1 : seed;
        risc201::AluResult res = alu.execute(risc201::AluOp::DIV, a, b);
        assert(res.value == (a / b));
    }

    std::cout << "[PASS] Unsigned Restoring Division tests (including 10,000 random vectors)\n";
}

void test_bitwise_and_shifts(risc201::Alu& alu) {
    // AND, OR, XOR, NOT, MOV
    assert(alu.execute(risc201::AluOp::AND, 0xFF00FF00, 0x0F0F0F0F).value == 0x0F000F00);
    assert(alu.execute(risc201::AluOp::OR,  0xFF00FF00, 0x00FF00FF).value == 0xFFFFFFFF);
    assert(alu.execute(risc201::AluOp::XOR, 0x12345678, 0x12345678).value == 0);
    assert(alu.execute(risc201::AluOp::NOT, 0x00000000, 0).value == 0xFFFFFFFF);
    assert(alu.execute(risc201::AluOp::MOV, 0xDEADBEEF, 0).value == 0xDEADBEEF);

    // SHL & SHR
    assert(alu.execute(risc201::AluOp::SHL, 1, 4).value == 16);
    assert(alu.execute(risc201::AluOp::SHR, 16, 2).value == 4);
    assert(alu.execute(risc201::AluOp::SHL, 1, 31).value == 0x80000000);
    assert(alu.execute(risc201::AluOp::SHL, 1, 31).flags.n == true);

    // Shift amount masking (b & 31): shift by 36 == shift by 4
    assert(alu.execute(risc201::AluOp::SHL, 1, 36).value == 16);

    std::cout << "[PASS] Bitwise & Shift operations tests\n";
}

int main() {
    risc201::Alu alu; // Standard direct non-virtual instantiation
    
    std::cout << "Running RISC201 ALU Verification Suite...\n";
    test_addition(alu);
    test_subtraction_and_cmp(alu);
    test_multiplication(alu);
    test_division(alu);
    test_bitwise_and_shifts(alu);
    
    std::cout << "\nAll ALU hardware algorithm tests PASSED successfully!\n";
    return 0;
}