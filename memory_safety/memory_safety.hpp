#pragma once

#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201 {

// safety class for all mem checks directly without virtuals
class MemorySafety {
 public:
  // check stack bounds for push pop
  ExceptionCode checkStack(uint32_t sp, bool isPush);

  // checks mem load store access
  ExceptionCode checkMem(uint32_t addr, bool isWrite);

  // verify if pc inside code region
  ExceptionCode checkPC(uint32_t pc);
};

// alias so keshav cpu and tests dont break
using MemorySafetyImpl = MemorySafety;

std::string exceptionToString(ExceptionCode code);

// draw ascii visual of stack memory
void renderStackAscii(const uint32_t* memory, uint32_t sp, uint32_t maxDepth = 16);

// tracks exception details for debug
class ExceptionTracker {
 public:
  ExceptionTracker() { reset(); }

  // save error details
  void logException(ExceptionCode code, uint32_t addr, uint64_t currentCycle);
  
  // clear tracker
  void reset();

  ExceptionCode getLastError() const { return lastError; }
  uint32_t getFaultAddress() const { return faultAddress; }
  uint64_t getCycle() const { return cycle; }

 private:
  ExceptionCode lastError;
  uint32_t faultAddress;
  uint64_t cycle;
};

}  // namespace risc201