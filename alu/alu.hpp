#pragma once

// The simulator calls execute() from the EX stage.

#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

class Alu {
 public:
  virtual ~Alu() = default;

  // Applies op to a and b. NOT and MOV only look at a.
  virtual AluResult execute(AluOp op, uint32_t a, uint32_t b) = 0;
};

}  // namespace risc201
