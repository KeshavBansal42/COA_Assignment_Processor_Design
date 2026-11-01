#include "alu/alu.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>

// Keep the original smoke checks active in optimized/NDEBUG builds too.
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "Check failed at line " << line << ": " << expression << '\n';
        std::abort();
    }
}
#define CHECK(condition) check((condition), #condition, __LINE__)

void test_addition(risc201::Alu& alu) {
    // Normal add
    risc201::AluResult r1 = alu.execute(risc201::AluOp::ADD, 10, 20);
    CHECK(r1.value == 30);
    CHECK(!r1.flags.z && !r1.flags.n && !r1.flags.c && !r1.flags.v);

    // Carry out without signed overflow: 0xFFFFFFFF + 1 = 0
    risc201::AluResult r2 = alu.execute(risc201::AluOp::ADD, 0xFFFFFFFF, 1);
    CHECK(r2.value == 0);
    CHECK(r2.flags.z == true);
    CHECK(r2.flags.c == true);
    CHECK(r2.flags.v == false);
    CHECK(r2.flags.n == false);

    // Signed overflow: 0x7FFFFFFF (+MAX) + 1 = 0x80000000 (-MIN)
    risc201::AluResult r3 = alu.execute(risc201::AluOp::ADD, 0x7FFFFFFF, 1);
    CHECK(r3.value == 0x80000000);
    CHECK(r3.flags.v == true);
    CHECK(r3.flags.n == true);
    CHECK(r3.flags.c == false);

    std::cout << "[PASS] CLA Addition & Flags tests\n";
}

void test_subtraction_and_cmp(risc201::Alu& alu) {
    // Normal sub: 20 - 10 = 10 (c=1: no borrow, a >= b)
    risc201::AluResult r1 = alu.execute(risc201::AluOp::SUB, 20, 10);
    CHECK(r1.value == 10);
    CHECK(!r1.flags.z && !r1.flags.n && r1.flags.c && !r1.flags.v);

    // Sub producing negative: 10 - 20 = -10 (c=0: borrow, a < b)
    risc201::AluResult r2 = alu.execute(risc201::AluOp::SUB, 10, 20);
    CHECK(r2.value == static_cast<uint32_t>(-10));
    CHECK(r2.flags.n == true);
    CHECK(r2.flags.c == false);
    CHECK(r2.flags.v == false);

    // Sub zero result: 15 - 15 = 0
    risc201::AluResult r3 = alu.execute(risc201::AluOp::SUB, 15, 15);
    CHECK(r3.value == 0);
    CHECK(r3.flags.z == true);
    CHECK(r3.flags.c == true);

    // Signed overflow on subtraction: 0x80000000 - 1 = 0x7FFFFFFF
    risc201::AluResult r4 = alu.execute(risc201::AluOp::SUB, 0x80000000, 1);
    CHECK(r4.value == 0x7FFFFFFF);
    CHECK(r4.flags.v == true);
    CHECK(r4.flags.n == false);

    // CMP sets flags identical to SUB
    risc201::AluResult r5 = alu.execute(risc201::AluOp::CMP, 42, 42);
    CHECK(r5.flags.z == true);
    CHECK(r5.flags.c == true);

    std::cout << "[PASS] CLA Subtraction & CMP tests\n";
}

void test_multiplication(risc201::Alu& alu) {
    // Positive * Positive
    risc201::AluResult r1 = alu.execute(risc201::AluOp::MUL, 6, 7);
    CHECK(r1.value == 42);
    CHECK(!r1.flags.v && !r1.flags.n && !r1.flags.z);

    // Positive * Negative: 5 * (-4) = -20
    risc201::AluResult r2 = alu.execute(risc201::AluOp::MUL, 5, static_cast<uint32_t>(-4));
    CHECK(r2.value == static_cast<uint32_t>(-20));
    CHECK(r2.flags.n == true);
    CHECK(r2.flags.v == false);

    // Negative * Negative: (-3) * (-4) = 12
    risc201::AluResult r3 = alu.execute(risc201::AluOp::MUL, static_cast<uint32_t>(-3), static_cast<uint32_t>(-4));
    CHECK(r3.value == 12);
    CHECK(!r3.flags.n && !r3.flags.v);

    // Multiply by zero
    risc201::AluResult r4 = alu.execute(risc201::AluOp::MUL, 0, 9999);
    CHECK(r4.value == 0);
    CHECK(r4.flags.z == true);

    // Signed overflow: 0x7FFFFFFF * 2
    risc201::AluResult r5 = alu.execute(risc201::AluOp::MUL, 0x7FFFFFFF, 2);
    CHECK(r5.flags.v == true);

    std::cout << "[PASS] Booth's Algorithm Multiplication tests\n";
}

void test_division(risc201::Alu& alu) {
    // Division by zero exception
    risc201::AluResult r_zero = alu.execute(risc201::AluOp::DIV, 10, 0);
    CHECK(r_zero.exception == risc201::ExceptionCode::DIV_BY_ZERO);

    // Standard unsigned division: 100 / 7 = 14
    risc201::AluResult r1 = alu.execute(risc201::AluOp::DIV, 100, 7);
    CHECK(r1.value == 14);
    CHECK(r1.exception == risc201::ExceptionCode::NONE);

    // A < B gives 0: 5 / 10 = 0
    risc201::AluResult r2 = alu.execute(risc201::AluOp::DIV, 5, 10);
    CHECK(r2.value == 0);
    CHECK(r2.flags.z == true);

    // Large unsigned numbers (> 2^31) testing 33-bit restoring accumulator:
    // 3,000,000,000 / 2 = 1,500,000,000
    risc201::AluResult r3 = alu.execute(risc201::AluOp::DIV, 3000000000u, 2u);
    CHECK(r3.value == 1500000000u);

    // 0xFFFFFFFF / 2 = 0x7FFFFFFF
    risc201::AluResult r4 = alu.execute(risc201::AluOp::DIV, 0xFFFFFFFFu, 2u);
    CHECK(r4.value == 0x7FFFFFFFu);

    // 0xFFFFFFFF / 0xFFFFFFFF = 1
    risc201::AluResult r5 = alu.execute(risc201::AluOp::DIV, 0xFFFFFFFFu, 0xFFFFFFFFu);
    CHECK(r5.value == 1u);

    // Deterministic C++ stress test: 10,000 pseudo-random divisions
    uint32_t seed = 123456789;
    for (int i = 0; i < 10000; i++) {
        seed = seed * 1664525u + 1013904223u;
        uint32_t a = seed;
        seed = seed * 1664525u + 1013904223u;
        uint32_t b = (seed == 0) ? 1 : seed;
        risc201::AluResult res = alu.execute(risc201::AluOp::DIV, a, b);
        CHECK(res.value == (a / b));
    }

    std::cout << "[PASS] Unsigned Restoring Division tests (including 10,000 random vectors)\n";
}

void test_bitwise_and_shifts(risc201::Alu& alu) {
    // AND, OR, XOR, NOT, MOV
    CHECK(alu.execute(risc201::AluOp::AND, 0xFF00FF00, 0x0F0F0F0F).value == 0x0F000F00);
    CHECK(alu.execute(risc201::AluOp::OR,  0xFF00FF00, 0x00FF00FF).value == 0xFFFFFFFF);
    CHECK(alu.execute(risc201::AluOp::XOR, 0x12345678, 0x12345678).value == 0);
    CHECK(alu.execute(risc201::AluOp::NOT, 0x00000000, 0).value == 0xFFFFFFFF);
    CHECK(alu.execute(risc201::AluOp::MOV, 0xDEADBEEF, 0).value == 0xDEADBEEF);

    // SHL & SHR
    CHECK(alu.execute(risc201::AluOp::SHL, 1, 4).value == 16);
    CHECK(alu.execute(risc201::AluOp::SHR, 16, 2).value == 4);
    CHECK(alu.execute(risc201::AluOp::SHL, 1, 31).value == 0x80000000);
    CHECK(alu.execute(risc201::AluOp::SHL, 1, 31).flags.n == true);

    // Shift amount masking (b & 31): shift by 36 == shift by 4
    CHECK(alu.execute(risc201::AluOp::SHL, 1, 36).value == 16);

    std::cout << "[PASS] Bitwise & Shift operations tests\n";
}

// Arithmetic failures include operands for reproducible diagnostics.
void require(bool condition, const char* what, uint32_t a, uint32_t b) {
    if (!condition) {
        std::cerr << what << " failed: a=0x" << std::hex << a
                  << " b=0x" << b << std::dec << '\n';
        std::abort();
    }
}

int64_t signed_value(uint32_t value) {
    return value < 0x80000000u ? int64_t{value} : int64_t{value} - (int64_t{1} << 32);
}

bool signed_overflow(int64_t value) {
    return value < -2147483648LL || value > 2147483647LL;
}

void check_result(const risc201::AluResult& result, uint32_t value, bool carry,
                  bool overflow, const char* what, uint32_t a, uint32_t b) {
    const risc201::Flags flags{value == 0, (value >> 31) != 0, carry, overflow};
    require(result.value == value && result.flags == flags &&
            result.exception == risc201::ExceptionCode::NONE, what, a, b);
}

void check_arithmetic(risc201::Alu& alu, uint32_t a, uint32_t b) {
    using namespace risc201;
    const int64_t sa = signed_value(a), sb = signed_value(b);
    const uint64_t sum = uint64_t{a} + b;
    check_result(alu.execute(AluOp::ADD, a, b), static_cast<uint32_t>(sum),
                 (sum >> 32) != 0, signed_overflow(sa + sb), "ADD", a, b);
    const auto sub = alu.execute(AluOp::SUB, a, b);
    check_result(sub, a - b, a >= b, signed_overflow(sa - sb), "SUB", a, b);
    const auto cmp = alu.execute(AluOp::CMP, a, b);
    require(cmp.value == sub.value && cmp.flags == sub.flags && cmp.exception == sub.exception,
            "CMP agrees with SUB", a, b);
    require((cmp.flags.n != cmp.flags.v) == (sa < sb), "signed branch condition", a, b);

    const int64_t product = sa * sb;
    require(detail::boothMultiply(a, b) == static_cast<uint64_t>(product),
            "full 64-bit Booth product", a, b);
    const auto mul = alu.execute(AluOp::MUL, a, b);
    check_result(mul, static_cast<uint32_t>(product), false, signed_overflow(product),
                 "MUL", a, b);
    const auto reversed = alu.execute(AluOp::MUL, b, a);
    require(mul.value == reversed.value && mul.flags == reversed.flags &&
            mul.exception == reversed.exception, "MUL operand symmetry", a, b);

    const auto div = alu.execute(AluOp::DIV, a, b);
    const auto internal = detail::restoringDivide(a, b);
    if (b == 0) {
        require(div.exception == ExceptionCode::DIV_BY_ZERO && div.value == 0 &&
                div.flags == (Flags{true, false, false, false}) &&
                internal.exception == ExceptionCode::DIV_BY_ZERO &&
                internal.quotient == 0 && internal.remainder == 0,
                "DIV by zero", a, b);
    } else {
        check_result(div, a / b, false, false, "unsigned DIV", a, b);
        require(internal.exception == ExceptionCode::NONE &&
                internal.quotient == div.value && internal.remainder == a % b &&
                internal.remainder < b &&
                uint64_t{internal.quotient} * b + internal.remainder == a,
                "restoring quotient/remainder invariant", a, b);
    }
}

void test_arithmetic_regressions(risc201::Alu& alu) {
    const std::array<uint32_t, 14> edges = {
        0, 1, 2, 3, 0x7ffffffeu, 0x7fffffffu, 0x80000000u,
        0x80000001u, 0xfffffffeu, 0xffffffffu, 0x55555555u,
        0xaaaaaaaau, 46340, 46341
    };
    for (auto a : edges) for (auto b : edges) check_arithmetic(alu, a, b);

    // Propagate carry/borrow across every bit boundary, including all CLA groups.
    for (unsigned bit = 1; bit <= 32; ++bit) {
        const uint32_t low = static_cast<uint32_t>((uint64_t{1} << bit) - 1);
        check_arithmetic(alu, low, 1);
        check_arithmetic(alu, 1, low);
        check_arithmetic(alu, ~low, 1);
        check_arithmetic(alu, low, low);
    }
    // Slide example: 147 / 11 = 13 remainder 4.
    check_arithmetic(alu, 147, 11);
    std::mt19937 random(201);
    for (int i = 0; i < 10000; ++i) {
        const uint32_t a = random(), b = random();
        check_arithmetic(alu, a, b);
        // Small divisors exercise long runs of nonzero quotient bits too.
        check_arithmetic(alu, a, (b & 0xffffu) + 1);
    }
    std::cout << "[PASS] Arithmetic boundaries, CLA carries, Booth symmetry, "
                 "restoring remainders and 20,000 random operand pairs\n";
}

void test_shared_datapath() {
    using namespace risc201;
    constexpr uint64_t mask = (uint64_t{1} << 33) - 1;
    const uint64_t words[] = {0, 1, 0x7fffffff, 0x80000000, 0xffffffff,
                              0x100000000, 0x1ffffffff};
    for (auto a : words) for (auto b : words) {
        CHECK(detail::addSubtract33(a, b, false) == ((a + b) & mask));
        CHECK(detail::addSubtract33(a, b, true) == ((a - b) & mask));
    }
    const int32_t exponents[] = {-512, -149, -126, -1, 0, 1, 127, 512};
    for (auto a : exponents) for (auto b : exponents) {
        CHECK(detail::addSigned32(a, b) == a + b);
        CHECK(detail::subtractSigned32(a, b) == a - b);
    }
    const uint32_t divisors[] = {1, 2, 3, 0x00800000, 0x00ffffff,
                                 0x7fffffff, 0x80000000, 0xffffffff};
    for (auto d : divisors) {
        const uint64_t partials[] = {0, uint64_t{d} - 1, d, uint64_t{d} * 2 - 1};
        for (auto partial : partials) {
            const auto step = detail::restoringStep(partial, d);
            CHECK(step.quotientBit == (partial >= d));
            CHECK(step.remainder == (partial >= d ? partial - d : partial));
            CHECK(step.remainder < d);
        }
    }
    CHECK(detail::boothMultiply(0x00ffffff, 0x00ffffff) == 0xfffffe000001ULL);
    std::cout << "[PASS] Shared CLA extensions, exponent arithmetic, full-width "
                 "significand product and restoring steps\n";
}

void test_logic_and_shift_regressions(risc201::Alu& alu) {
    using namespace risc201;
    const uint32_t values[] = {0, 1, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0xaaaaaaaa};
    const uint32_t counts[] = {0, 1, 2, 31, 32, 33, 36, 63, 64, 0xffffffffu};
    for (auto a : values) {
        for (auto b : values) {
            check_result(alu.execute(AluOp::AND, a, b), a & b, false, false, "AND", a, b);
            check_result(alu.execute(AluOp::OR, a, b), a | b, false, false, "OR", a, b);
            check_result(alu.execute(AluOp::XOR, a, b), a ^ b, false, false, "XOR", a, b);
            check_result(alu.execute(AluOp::NOT, a, b), ~a, false, false, "NOT ignores B", a, b);
            check_result(alu.execute(AluOp::MOV, a, b), a, false, false, "MOV ignores B", a, b);
        }
        for (auto b : counts) {
            const unsigned shift = b & 31u;
            const uint64_t factor = uint64_t{1} << shift;
            check_result(alu.execute(AluOp::SHL, a, b), static_cast<uint32_t>(uint64_t{a} * factor),
                         false, false, "SHL modulo 32", a, b);
            check_result(alu.execute(AluOp::SHR, a, b), static_cast<uint32_t>(uint64_t{a} / factor),
                         false, false, "logical SHR modulo 32", a, b);
        }
    }
    std::cout << "[PASS] Logic flags, unary operands and masked logical shifts\n";
}

int main() {
    risc201::Alu alu; // Standard direct non-virtual instantiation

    std::cout << "Running RISC201 ALU Verification Suite...\n";
    test_addition(alu);
    test_subtraction_and_cmp(alu);
    test_multiplication(alu);
    test_division(alu);
    test_bitwise_and_shifts(alu);
    test_arithmetic_regressions(alu);
    test_shared_datapath();
    test_logic_and_shift_regressions(alu);

    std::cout << "\nAll ALU hardware algorithm tests PASSED successfully!\n";
    return 0;
}
