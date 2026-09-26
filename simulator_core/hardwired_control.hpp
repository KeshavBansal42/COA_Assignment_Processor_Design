#pragma once

// Hardwired decode, the stand-in for the microcode control unit.

#include <cstdint>

#include "control_unit/control_unit.hpp"
#include "isa_spec/isa201.h"

namespace risc201 {

class HardwiredControl : public ControlUnit {
 public:
  ControlSignals decode(uint32_t instr) override;
  uint32_t getMicroPC() const override;
};

}  // namespace risc201
