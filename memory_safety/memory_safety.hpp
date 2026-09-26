#pragma once

#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201 {

// safety class for all mem checks
class MemorySafety {
 public:
  virtual ~MemorySafety() = default;

  // check stack bounds for push pop
  virtual ExceptionCode checkStack(uint32_t sp, bool isPush) = 0;

  // checks mem load store access
  virtual ExceptionCode checkMem(uint32_t addr, bool isWrite) = 0;

  // verify if pc inside code region
  virtual ExceptionCode checkPC(uint32_t pc) = 0;
};

// real implementation for simulator
class MemorySafetyImpl : public MemorySafety {
 public:
  ExceptionCode checkStack(uint32_t sp, bool isPush) override;
  ExceptionCode checkMem(uint32_t addr, bool isWrite) override;
  ExceptionCode checkPC(uint32_t pc) override;
};

std::string exceptionToString(ExceptionCode code);

}  // namespace risc201