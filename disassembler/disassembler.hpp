#pragma once

// disassemble() and assemble() have to round trip.

#include <cstdint>
#include <string>

#include "isa_spec/isa201.h"

namespace risc201 {

class Disassembler {
 public:
  virtual ~Disassembler() = default;

  // Returns one text line for the instruction, for example "ADD r1, r2, r3".
  virtual std::string disassemble(uint32_t word) = 0;
};

}  // namespace risc201
