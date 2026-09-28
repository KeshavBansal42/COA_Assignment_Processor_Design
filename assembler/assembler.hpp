#pragma once

// Errors come back as std::runtime_error with the line number in the message.

#include <cstdint>
#include <string>
#include <vector>

#include "isa_spec/isa201.h"

namespace risc201 {

class Assembler {
 public:
  virtual ~Assembler() = default;

  // Reads a .s file and returns the encoded program, one word per instruction.
  virtual std::vector<uint32_t> assemble(const std::string& path) = 0;
};

}  // namespace risc201