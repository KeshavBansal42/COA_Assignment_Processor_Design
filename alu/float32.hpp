#pragma once

#include <cstdint>

namespace risc201::fp32 {

enum class RoundingMode {
    NearestEven,
    TowardZero,
    TowardPositive,
    TowardNegative,
};

// Per-operation IEEE arithmetic status. These do not trap, modify integer
// FLAGS, or accumulate globally. Underflow uses tininess after rounding.
struct Status {
    bool invalid = false;
    bool divideByZero = false;
    bool overflow = false;
    bool underflow = false;
    bool inexact = false;
};

struct Result {
    uint32_t value = 0;
    Status status{};
};

// Operands/results are raw IEEE binary32 words. All NaNs return the canonical
// quiet NaN 0x7FC00000; signaling NaN inputs also set invalid. Arithmetic uses
// shared CLA/Booth/restoring primitives from alu.cpp, never Alu::execute recursion.
Result add(uint32_t a, uint32_t b, RoundingMode mode = RoundingMode::NearestEven);
Result subtract(uint32_t a, uint32_t b, RoundingMode mode = RoundingMode::NearestEven);
Result multiply(uint32_t a, uint32_t b, RoundingMode mode = RoundingMode::NearestEven);
Result divide(uint32_t a, uint32_t b, RoundingMode mode = RoundingMode::NearestEven);

} // namespace risc201::fp32
