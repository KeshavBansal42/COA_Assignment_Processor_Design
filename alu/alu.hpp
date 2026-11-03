#pragma once

#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201 {

namespace detail {

// Shared datapath primitives. They never commit CPU FLAGS or FP status.
uint32_t claAdd(uint32_t a, uint32_t b, uint32_t carryIn, bool& carryOut, bool& overflow);
uint32_t add32(uint32_t a, uint32_t b);
uint32_t subtract32(uint32_t a, uint32_t b);
// Two 32-bit CLA blocks chained by their carry (arithmetic modulo 2^64).
uint64_t add64(uint64_t a, uint64_t b);
uint64_t subtract64(uint64_t a, uint64_t b);
int32_t addSigned32(int32_t a, int32_t b);
int32_t subtractSigned32(int32_t a, int32_t b);

// Low 33 bits of a +/- b, using the 32-bit CLA plus one extension bit.
uint64_t addSubtract33(uint64_t a, uint64_t b, bool subtract);

// Baseline reference: complete signed 32x32 product as 64 two's-complement bits.
// Active integer/FP instructions use Wallace multiplication below.
uint64_t boothMultiply(uint32_t a, uint32_t b);

struct CarrySaveResult {
    uint64_t sum;
    uint64_t carry; // Already shifted left by one bit.
};

// Three rows -> two, with no carry propagation; equality is modulo 2^64.
CarrySaveResult carrySave(uint64_t a, uint64_t b, uint64_t c);
uint64_t wallaceMultiply(uint32_t a, uint32_t b); // Unsigned, complete product.
uint64_t signedWallaceMultiply(uint32_t a, uint32_t b); // Two's-complement product.

struct DivisionStep {
    uint64_t remainder;
    bool quotientBit;
};

// One subtract/test/restore step. divisor != 0 and partial < 2*divisor.
// Integer DIV builds its quotient with this step; FDIV uses Newton below.
DivisionStep restoringStep(uint64_t partial, uint32_t divisor);

// Internal algorithm result for direct arithmetic verification.
// The processor's DIV instruction still returns only the quotient.
struct UnsignedDivisionResult {
    uint32_t quotient;
    uint32_t remainder;
    ExceptionCode exception;
};

UnsignedDivisionResult restoringDivide(uint32_t dividend, uint32_t divisor);

// Q1.31 reciprocal for a normalized 24-bit divisor in [2^23,2^24).
uint32_t newtonReciprocal(uint32_t divisor);
// Exact floor(numerator * 2^26 / divisor) and remainder, after NR correction.
// Preconditions: normalized divisor; divisor <= numerator < 2*divisor.
UnsignedDivisionResult newtonDivideSignificands(uint32_t numerator, uint32_t divisor);

} // namespace detail

class Alu {
public:
    // Applies op to a and b. NOT and MOV only look at a. MUL is signed;
    // DIV is unsigned. Returned flags are committed only when control requests
    // writeFlags. Floating operations use binary32 nearest-even; fp32's API
    // exposes other modes and FP status. See docs/alu.md for the full contract.
    AluResult execute(AluOp op, uint32_t a, uint32_t b);

private:
    // Structural gate-level hardware simulations
    uint32_t wallace_mul(uint32_t a, uint32_t b, bool& overflow);
    uint32_t unsigned_div(uint32_t dividend, uint32_t divisor, ExceptionCode& exc);
};

using ConcreteAlu = Alu;

} // namespace risc201
