#include "memory_safety/memory_safety.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

namespace risc201 {

// check if mem access is valid
ExceptionCode MemorySafety::checkMem(uint32_t addr, bool isWrite) {
  // block if address more than 0x0FFF
  if (addr > kLastAddress) {
    return ExceptionCode::INVALID_MEM_ACCESS;
  }

  // cant write to code region 
  if (isWrite && addr <= kCodeLimit) {
    return ExceptionCode::INVALID_MEM_ACCESS;
  }

  return ExceptionCode::NONE;
}

// check stack boundries
ExceptionCode MemorySafety::checkStack(uint32_t sp, bool isPush) {
  if (isPush) {
    // push decreases sp so check if it crosses the heapTop_ boundary
    if (sp == 0 || (sp - 1) < heapTop_) {
      return ExceptionCode::STACK_OVERFLOW;
    }
  } else {
    // pop increases sp so check if it crosses initial SP limit
    if (sp >= kInitialSp) {
      return ExceptionCode::STACK_UNDERFLOW;
    }
  }

  return ExceptionCode::NONE;
}

// check if pc is inside code bounds
ExceptionCode MemorySafety::checkPC(uint32_t pc) {
  // pc cant go above code limit 0x07FF
  if (pc > kCodeLimit) {
    return ExceptionCode::PC_OUT_OF_RANGE;
  }
  return ExceptionCode::NONE;
}

// ---- Dynamic heap and stack boundary management ----

void MemorySafety::enableDynamicStack(bool enable) {
  dynamicStackEnabled_ = enable;
  if (enable) {
    // When dynamic stack is enabled, stack floor is driven by actual heap allocation
    heapTop_ = std::max(heapAllocTop_, kDataBase);
  } else {
    // Revert to fixed 0x0C00 boundary
    heapTop_ = kStackBase;
  }
}

void MemorySafety::setHeapTop(uint32_t newTop) {
  if (newTop < kDataBase) {
    newTop = kDataBase;
  }
  if (newTop > kStackLimit) {
    newTop = kStackLimit;
  }
  heapTop_ = newTop;
}

uint32_t MemorySafety::allocHeap(uint32_t words, uint32_t currentSp) {
  if (words == 0) {
    return heapAllocTop_;
  }
  // Check for collision with stack pointer (or end of memory space)
  if (heapAllocTop_ + words >= currentSp || heapAllocTop_ + words > kStackLimit) {
    return 0; // collision detected or out of memory
  }
  uint32_t allocatedAddr = heapAllocTop_;
  heapAllocTop_ += words;
  if (dynamicStackEnabled_ || heapAllocTop_ > heapTop_) {
    heapTop_ = heapAllocTop_;
  }
  return allocatedAddr;
}

bool MemorySafety::freeHeap(uint32_t words) {
  if (words > (heapAllocTop_ - kDataBase)) {
    heapAllocTop_ = kDataBase;
  } else {
    heapAllocTop_ -= words;
  }
  if (dynamicStackEnabled_) {
    heapTop_ = heapAllocTop_;
  }
  return true;
}

uint32_t MemorySafety::heapSize() const {
  return heapAllocTop_ > kDataBase ? (heapAllocTop_ - kDataBase) : 0;
}

uint32_t MemorySafety::stackDepth(uint32_t sp) const {
  return (sp <= kInitialSp) ? (kInitialSp - sp) : 0;
}

uint32_t MemorySafety::availableFreeWords(uint32_t sp) const {
  return (sp > heapTop_) ? (sp - heapTop_) : 0;
}

uint32_t MemorySafety::stackCapacity(uint32_t sp) const {
  return availableFreeWords(sp);
}

// convert exception to nice readable string
std::string exceptionToString(ExceptionCode code) {
  switch (code) {
    case ExceptionCode::NONE:
      return "No exception";
    case ExceptionCode::STACK_OVERFLOW:
      return "Stack overflow (SP went below 0xC00)";
    case ExceptionCode::STACK_UNDERFLOW:
      return "Stack underflow (SP went above 0x1000)";
    case ExceptionCode::INVALID_MEM_ACCESS:
      return "Invalid memory access (out of bounds or code overwrite)";
    case ExceptionCode::DIV_BY_ZERO:
      return "Division by zero";
    case ExceptionCode::ILLEGAL_INSTRUCTION:
      return "Illegal instruction encountered";
    case ExceptionCode::PC_OUT_OF_RANGE:
      return "Program counter went out of code region";
  }
  return "Unknown error";
}

// save exception info
void ExceptionTracker::logException(ExceptionCode code, uint32_t addr, uint64_t currentCycle) {
  lastError = code;
  faultAddress = addr;
  cycle = currentCycle;
  history_.push_back({code, addr, currentCycle});
}

// clear all error states
void ExceptionTracker::reset() {
  lastError = ExceptionCode::NONE;
  faultAddress = 0;
  cycle = 0;
  history_.clear();
}

// check if pc is inside code bounds
ExceptionCode MemorySafety::checkPC(uint32_t pc) {
  // pc cant go above code limit 0x07FF
  if (pc > 0x07FF) {
    return ExceptionCode::PC_OUT_OF_RANGE;
  }
  return ExceptionCode::NONE;
}

// render ascii stack frame
void renderStackAscii(const uint32_t* memory, uint32_t sp, uint32_t maxDepth) {
  // print warning in red if sp cross danger line
  if (sp < kStackBase) {
    out << "\033[31m*** STACK OVERFLOW DANGER ***\033[0m\n";
  }

  out << "+----------------- STACK -----------------+\n";
  
  // calculate start address to center around sp
  uint32_t startAddr = sp + (maxDepth / 2);
  
  // cant go above initial stack pointer
  if (startAddr > kInitialSp) {
    startAddr = kInitialSp;
  }
  
  for (uint32_t i = 0; i < maxDepth; i++) {
    if (startAddr == 0) break;
    uint32_t currentAddr = startAddr - 1 - i;
    
    // stop printing if we cross bottom boundary
    if (currentAddr < kDataBase) break;
    
    uint32_t val = memory[currentAddr];
    
    // draw box with hex and dec values
    out << "| 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << currentAddr << " | ";
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << val << " | ";
    out << std::dec << std::setw(10) << std::setfill(' ') << val << " |";
    
    // add pointer marker for sp
    if (currentAddr == sp) {
      out << " <-- SP";
    }
    
    out << "\n";

    // guess frame boundry if value looks like a return address (code region)
    if (val > 0 && val <= kCodeLimit) {
      out << "+----------------- FRAME -----------------+\n";
    }
  }
  
  out << "+-----------------------------------------+\n";
}

// render ascii stack frame to std::cout (backward compatible)
void renderStackAscii(const uint32_t* memory, uint32_t sp, uint32_t maxDepth) {
  renderStackAscii(std::cout, memory, sp, maxDepth);
}

}  // namespace risc201
