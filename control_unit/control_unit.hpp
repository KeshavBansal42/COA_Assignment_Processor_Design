#pragma once

// The hardwired decode goes in first, microcode replaces it later.

#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

class ControlUnit {
 public:
  virtual ~ControlUnit() = default;

  // Turns one instruction word into the signals the pipeline needs.
  virtual ControlSignals decode(uint32_t instr) = 0;

  // Which microinstruction the current instruction is on, 0 while decode is hardwired.
  virtual uint32_t getMicroPC() const = 0;
};

}  // namespace risc201
