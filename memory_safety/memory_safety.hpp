#pragma once

// Memory safety interface.

#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

class MemorySafety {
 public:
  virtual ~MemorySafety() = default;

  // Checks a PUSH/CALL (isPush true) or a POP/RET against the stack limits.
  // Returns NONE when the access is legal.
  virtual ExceptionCode checkStack(uint32_t sp, bool isPush) = 0;

  // Checks a load or store against the memory map.
  // Returns NONE when the access is legal.
  virtual ExceptionCode checkMem(uint32_t addr, bool isWrite) = 0;
};

}  // namespace risc201
