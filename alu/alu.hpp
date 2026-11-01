#pragma once

#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201 {

namespace detail {

// Shared datapath primitives. They never commit CPU FLAGS or FP status.
uint32_t claAdd(uint32_t a, uint32_t b, uint32_t carryIn, bool& carryOut, bool& overflow);
uint32_t add32(uint32_t a, uint32_t b);
uint32_t subtract32(uint32_t a, uint32_t b);
int32_t addSigned32(int32_t a, int32_t b);
int32_t subtractSigned32(int32_t a, int32_t b);

// Low 33 bits of a +/- b, using the 32-bit CLA plus one extension bit.
uint64_t addSubtract33(uint64_t a, uint64_t b, bool subtract);

// Complete signed 32x32 product, represented as 64 two's-complement bits.
// Integer MUL selects the low word; FP MUL retains all 48 significand bits.
uint64_t boothMultiply(uint32_t a, uint32_t b);

struct DivisionStep {
    uint64_t remainder;
    bool quotientBit;
};

// One subtract/test/restore step. divisor != 0 and partial < 2*divisor.
// Both integer and fractional division build their quotient with this step.
DivisionStep restoringStep(uint64_t partial, uint32_t divisor);

// Internal algorithm result for direct arithmetic verification.
// The processor's DIV instruction still returns only the quotient.
struct UnsignedDivisionResult {
    uint32_t quotient;
    uint32_t remainder;
    ExceptionCode exception;
};

UnsignedDivisionResult restoringDivide(uint32_t dividend, uint32_t divisor);

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
    uint32_t booth_mul(uint32_t a, uint32_t b, bool& overflow);
    uint32_t unsigned_div(uint32_t dividend, uint32_t divisor, ExceptionCode& exc);
};

using ConcreteAlu = Alu;

} // namespace risc201
