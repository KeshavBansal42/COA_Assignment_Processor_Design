#include "alu/float32.hpp"
#include "alu/alu.hpp"

#include <array>
#include <cfenv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>

using namespace risc201;
using namespace risc201::fp32;

namespace {

using Function = Result (*)(uint32_t, uint32_t, RoundingMode);
constexpr Function functions[] = {fp32::add, fp32::subtract, fp32::multiply, fp32::divide};
constexpr AluOp operations[] = {AluOp::FADD, AluOp::FSUB, AluOp::FMUL, AluOp::FDIV};
constexpr RoundingMode modes[] = {RoundingMode::NearestEven, RoundingMode::TowardZero,
                                  RoundingMode::TowardPositive, RoundingMode::TowardNegative};
constexpr int hostModes[] = {FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD, FE_DOWNWARD};
constexpr unsigned invalid = 1, divideZero = 2, overflow = 4, underflow = 8, inexact = 16;

unsigned statusBits(const Status& s) {
    return unsigned(s.invalid) | (unsigned(s.divideByZero) << 1) |
           (unsigned(s.overflow) << 2) | (unsigned(s.underflow) << 3) |
           (unsigned(s.inexact) << 4);
}

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::abort();
    }
}

void expect(unsigned op, uint32_t a, uint32_t b, unsigned mode,
            uint32_t value, unsigned status) {
    const auto result = functions[op](a, b, modes[mode]);
    if (result.value != value || statusBits(result.status) != status) {
        std::cerr << "FP mismatch op=" << op << " mode=" << mode << std::hex
                  << " a=" << a << " b=" << b << " got=" << result.value
                  << " expected=" << value << " status=" << statusBits(result.status)
                  << " expected status=" << status << std::dec << '\n';
        std::abort();
    }
}

void directedTests() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        // Exact basic operations, including the addition worked example in slides.
        expect(0, 0x41200000, 0x40e00000, mode, 0x41880000, 0); // 10 + 7 = 17
        expect(1, 0x40600000, 0x40000000, mode, 0x3fc00000, 0); // 3.5 - 2 = 1.5
        expect(2, 0xbfc00000, 0x40000000, mode, 0xc0400000, 0);
        expect(3, 0x40400000, 0x40000000, mode, 0x3fc00000, 0);
        // Recurring quotients and reciprocals immediately around one exercise
        // Newton precision plus final exact residual correction/rounding.
        expect(3, 0x3f800000, 0x40400000, mode,
               mode == 0 || mode == 2 ? 0x3eaaaaab : 0x3eaaaaaa, inexact);
        expect(3, 0xbf800000, 0x40400000, mode,
               mode == 0 || mode == 3 ? 0xbeaaaaab : 0xbeaaaaaa, inexact);
        expect(3, 0x3f800000, 0x3f800001, mode,
               mode == 2 ? 0x3f7fffff : 0x3f7ffffe, inexact);
        expect(3, 0x3f800000, 0x3f7fffff, mode,
               mode == 0 || mode == 2 ? 0x3f800001 : 0x3f800000, inexact);
        // Halfway cases: even retained LSB stays, odd retained LSB increments.
        expect(0, 0x3f800000, 0x33800000, mode,
               mode == 2 ? 0x3f800001 : 0x3f800000, inexact);
        expect(0, 0x3f800001, 0x33800000, mode,
               mode == 0 || mode == 2 ? 0x3f800002 : 0x3f800001, inexact);
        expect(0, 0xbf800000, 0xb3800000, mode,
               mode == 3 ? 0xbf800001 : 0xbf800000, inexact);
        // A tiny alignment operand must survive in sticky, even beyond 64 bits.
        expect(0, 0x3f800000, 1, mode, mode == 2 ? 0x3f800001 : 0x3f800000, inexact);
        expect(1, 0x3f800000, 1, mode,
               mode == 1 || mode == 3 ? 0x3f7fffff : 0x3f800000, inexact);
        // Exact subnormals do not raise underflow; tiny inexact results do.
        expect(3, 0x00800000, 0x40000000, mode, 0x00400000, 0);
        expect(0, 0x007fffff, 1, mode, 0x00800000, 0);
        expect(3, 1, 0x40000000, mode, mode == 2 ? 1 : 0, underflow | inexact);
        expect(3, 0x80000001, 0x40000000, mode,
               mode == 3 ? 0x80000001 : 0x80000000, underflow | inexact);
        // Exactly halfway below minimum normal: tininess is tested AFTER rounding.
        const bool normal = mode == 0 || mode == 2;
        expect(2, 0x00800000, 0x3f7fffff, mode,
               normal ? 0x00800000 : 0x007fffff, inexact | (normal ? 0 : underflow));
        expect(2, 0x7f7fffff, 0x40000000, mode,
               mode == 0 || mode == 2 ? 0x7f800000 : 0x7f7fffff, overflow | inexact);
        expect(2, 0xff7fffff, 0x40000000, mode,
               mode == 0 || mode == 3 ? 0xff800000 : 0xff7fffff, overflow | inexact);

        expect(0, 0x3f800000, 0xbf800000, mode, mode == 3 ? 0x80000000 : 0, 0);
        expect(0, 0x80000000, 0x80000000, mode, 0x80000000, 0);
        expect(1, 0, 0x80000000, mode, 0, 0);
        expect(2, 0x80000000, 0x40000000, mode, 0x80000000, 0);
        expect(3, 0xbf800000, 0, mode, 0xff800000, divideZero);
        expect(3, 0x3f800000, 0xff800000, mode, 0x80000000, 0);
        expect(3, 0x7f800000, 0, mode, 0x7f800000, 0);
        expect(0, 0x7f800000, 0xff800000, mode, 0x7fc00000, invalid);
        expect(1, 0x7f800000, 0x7f800000, mode, 0x7fc00000, invalid);
        expect(2, 0, 0x7f800000, mode, 0x7fc00000, invalid);
        expect(3, 0, 0, mode, 0x7fc00000, invalid);
        expect(3, 0x7f800000, 0xff800000, mode, 0x7fc00000, invalid);
        for (unsigned op = 0; op < 4; ++op) {
            expect(op, 0xffc12345, 0x3f800000, mode, 0x7fc00000, 0);
            expect(op, 0x7f812345, 0x3f800000, mode, 0x7fc00000, invalid);
            expect(op, 0x7fc12345, 0xff812345, mode, 0x7fc00000, invalid);
        }
    }
}

uint32_t bits(float value) {
    uint32_t word;
    std::memcpy(&word, &value, sizeof(word));
    return word;
}

float asFloat(uint32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

// Volatile operands/result and strict compiler options force actual binary32
// operations under the current fenv, independent of the integer implementation.
Result hostReference(unsigned op, uint32_t a, uint32_t b) {
    volatile float left = asFloat(a), right = asFloat(b), value;
    std::feclearexcept(FE_ALL_EXCEPT);
    switch (op) {
        case 0: value = left + right; break;
        case 1: value = left - right; break;
        case 2: value = left * right; break;
        default: value = left / right; break;
    }
    const int flags = std::fetestexcept(FE_ALL_EXCEPT);
    uint32_t word = bits(value);
    if ((word & 0x7fffffffu) > 0x7f800000u) word = 0x7fc00000;
    return {word, {bool(flags & FE_INVALID), bool(flags & FE_DIVBYZERO),
                   bool(flags & FE_OVERFLOW), bool(flags & FE_UNDERFLOW), bool(flags & FE_INEXACT)}};
}

void compareHost(uint32_t a, uint32_t b, unsigned mode) {
    for (unsigned op = 0; op < 4; ++op) {
        const auto expected = hostReference(op, a, b);
        // Some hosts detect tininess before rounding; the module deliberately
        // detects it afterwards. Normalize only that documented policy difference.
        unsigned flags = statusBits(expected.status);
        if ((expected.value & 0x7fffffffu) >= 0x00800000u) flags &= ~underflow;
        expect(op, a, b, mode, expected.value, flags);
        if (mode == 0) {
            Alu alu;
            const auto actual = alu.execute(operations[op], a, b);
            check(actual.value == expected.value && actual.exception == ExceptionCode::NONE,
                  "ALU floating dispatch changed value or raised an integer exception");
        }
    }
}

void differentialTests() {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
                  std::numeric_limits<float>::digits == 24, "host oracle needs binary32");
    const uint32_t edges[] = {0, 0x80000000, 1, 0x80000001, 0x007fffff, 0x807fffff,
        0x00800000, 0x00800001, 0x3f000000, 0x3f7fffff, 0x3f800000, 0x3f800001,
        0xbf800000, 0x40000000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000,
        0x7fc00000, 0x7f800001, 0xff800001};
    const int original = std::fegetround();
    for (unsigned mode = 0; mode < 4; ++mode) {
        check(std::fesetround(hostModes[mode]) == 0, "host does not support rounding mode");
        for (auto a : edges) for (auto b : edges) compareHost(a, b, mode);
        std::mt19937 rng(754);
        for (unsigned i = 0; i < 25000; ++i) {
            const uint32_t a = rng(), b = rng();
            compareHost(a, b, mode);
            // Target subnormals, close cancellation and adjacent exponents too.
            if (i < 2000) {
                compareHost(a & 0x807fffffu, b & 0x807fffffu, mode);
                compareHost(a, a ^ 0x80000000u, mode);
                compareHost((a & 0x807fffffu) | 0x3f800000u,
                            (b & 0x807fffffu) | 0x3f000000u, mode);
            }
        }
    }
    check(std::fesetround(original) == 0, "could not restore rounding mode");
    std::feclearexcept(FE_ALL_EXCEPT);
}

void hostIndependenceTest() {
    const int original = std::fegetround();
    for (auto hostMode : hostModes) {
        check(std::fesetround(hostMode) == 0, "could not select host rounding mode");
        std::feclearexcept(FE_ALL_EXCEPT);
        std::feraiseexcept(FE_INVALID);
        expect(0, 0x3f800000, 0x33800000, 0, 0x3f800000, inexact);
        expect(3, 0x3f800000, 0, 0, 0x7f800000, divideZero);
        check(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID, "software FP changed host status");
    }
    std::fesetround(original);
    std::feclearexcept(FE_ALL_EXCEPT);
}

} // namespace

int main() {
    directedTests();
    hostIndependenceTest();
    differentialTests();
    std::cout << "binary32 directed cases and 503,056 host comparisons passed "
                 "(four operations, four rounding modes)\n";
}
