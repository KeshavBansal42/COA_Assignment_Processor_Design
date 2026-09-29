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