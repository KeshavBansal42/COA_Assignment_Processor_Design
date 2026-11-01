#pragma once

#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201::detail {

// Internal algorithm result, exposed here for direct arithmetic verification.
// The processor's DIV instruction still returns only the quotient.
struct UnsignedDivisionResult {
    uint32_t quotient;
    uint32_t remainder;
    ExceptionCode exception;
};

UnsignedDivisionResult restoringDivide(uint32_t dividend, uint32_t divisor);

} // namespace risc201::detail
