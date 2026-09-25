#pragma once

// Stack and memory range checks against the memory map.

#include <cstdint>

#include "isa_spec/isa201.h"
#include "memory_safety/memory_safety.hpp"

namespace risc201 {

class SimpleSafety : public MemorySafety {
 public:
  ExceptionCode checkStack(uint32_t sp, bool isPush) override;
  ExceptionCode checkMem(uint32_t addr, bool isWrite) override;
};

}  // namespace risc201
