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
  MemorySafety() = default;

  // check stack bounds for push pop (against current heapTop_ floor)
  ExceptionCode checkStack(uint32_t sp, bool isPush);

  // checks mem load store access
  ExceptionCode checkMem(uint32_t addr, bool isWrite);

  // verify if pc inside code region
  ExceptionCode checkPC(uint32_t pc);

  // ---- Dynamic heap and stack boundary management ----

  // Toggle dynamic stack mode:
  // When enabled: if heap is empty, heapTop_ is kDataBase (0x800), allowing stack
  // to grow through the data region (2048 words total).
  // When disabled: fixed boundary at kStackBase (0x0C00) (1024 words stack).
  void enableDynamicStack(bool enable);
  bool isDynamicStackEnabled() const { return dynamicStackEnabled_; }

  // Set heap high-water mark manually (clamped within [kDataBase, kStackLimit]).
  void setHeapTop(uint32_t newTop);
  uint32_t heapTop() const { return heapTop_; }

  // Dynamic heap allocator: allocates 'words' on the heap from heapAllocTop_ upwards.
  // Performs collision detection against the current stack pointer ('currentSp').
  // Returns base address of the allocated block, or 0 on collision / failure.
  uint32_t allocHeap(uint32_t words, uint32_t currentSp);

  // Frees 'words' words from top of heap (will not shrink below kDataBase).
  bool freeHeap(uint32_t words);

  // Sizing metrics
  uint32_t heapSize() const;                      // words allocated in heap
  uint32_t stackDepth(uint32_t sp) const;         // words pushed on stack
  uint32_t availableFreeWords(uint32_t sp) const; // free words between heapTop and SP
  uint32_t stackCapacity(uint32_t sp) const;      // words stack can grow before collision

 private:
  bool dynamicStackEnabled_ = false;
  uint32_t heapAllocTop_ = kDataBase;  // current allocated end of heap (starts at 0x800)
  uint32_t heapTop_ = kStackBase;      // stack boundary floor (default 0x0C00 for backward compatibility)
};

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
