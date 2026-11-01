#pragma once

#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201 {

class Alu {
public:
    // Applies op to a and b. NOT and MOV only look at a. MUL is signed;
    // DIV is unsigned. Returned flags are committed only when control requests
    // writeFlags. Floating operations use binary32 nearest-even; fp32's API
    // exposes other modes and FP status. See docs/alu.md for the full contract.
    AluResult execute(AluOp op, uint32_t a, uint32_t b);

private:
    // Structural gate-level hardware simulations
    uint32_t cla_add(uint32_t a, uint32_t b, uint32_t c_in, bool& cout, bool& overflow);
    uint32_t booth_mul(uint32_t a, uint32_t b, bool& overflow);
    uint32_t unsigned_div(uint32_t dividend, uint32_t divisor, ExceptionCode& exc);
};

using ConcreteAlu = Alu;

} // namespace risc201
