#pragma once

// Straightforward ALU, the stand-in for the real one.

#include <cstdint>

#include "alu/alu.hpp"
#include "isa_spec/isa201.h"

namespace risc201 {

class SimpleAlu : public Alu {
 public:
  AluResult execute(AluOp op, uint32_t a, uint32_t b) override;
};

}  // namespace risc201
