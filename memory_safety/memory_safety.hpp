#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

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

// alias so keshav cpu and tests dont break without needing any changes to Keshav's files
using MemorySafetyImpl = MemorySafety;

std::string exceptionToString(ExceptionCode code);

// draw ascii visual of stack memory to a specific stream (redirectable / testable)
void renderStackAscii(std::ostream& out, const uint32_t* memory, uint32_t sp, uint32_t maxDepth = 16);

// overload writing to std::cout for backwards compatibility with Keshav's CLI
void renderStackAscii(const uint32_t* memory, uint32_t sp, uint32_t maxDepth = 16);

// One logged exception entry
struct ExceptionRecord {
  ExceptionCode code = ExceptionCode::NONE;
  uint32_t faultAddress = 0;
  uint64_t cycle = 0;
};

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

  // full history of all exceptions recorded
  const std::vector<ExceptionRecord>& history() const { return history_; }

 private:
  ExceptionCode lastError = ExceptionCode::NONE;
  uint32_t faultAddress = 0;
  uint64_t cycle = 0;
  std::vector<ExceptionRecord> history_;
};

}  // namespace risc201
