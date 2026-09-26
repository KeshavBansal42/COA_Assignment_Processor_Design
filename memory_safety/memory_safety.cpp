#include "memory_safety/memory_safety.h"
#include <string>
#include <iostream>
#include <iomanip>

namespace risc201 {

// check if mem access is valid
ExceptionCode MemorySafetyImpl::checkMem(uint32_t addr, bool isWrite) {
  // block if address more than 0x0FFF
  if (addr > 0x0FFF) {
    return ExceptionCode::INVALID_MEM_ACCESS;
  }

  // cant write to code region 
  if (isWrite && addr <= 0x07FF) {
    return ExceptionCode::INVALID_MEM_ACCESS;
  }

  return ExceptionCode::NONE;
}

// check stack boundries
ExceptionCode MemorySafetyImpl::checkStack(uint32_t sp, bool isPush) {
  if (isPush) {
    // push decrease sp so check if it cross 0x0C00
    if ((sp - 1) < 0x0C00) {
      return ExceptionCode::STACK_OVERFLOW;
    }
  } else {
    // pop increase sp so check if it cross 0x1000 limit
    if (sp >= 0x1000) {
      return ExceptionCode::STACK_UNDERFLOW;
    }
  }

  return ExceptionCode::NONE;
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

}  // namespace risc201

// save exception info
void ExceptionTracker::logException(ExceptionCode code, uint32_t addr, uint64_t currentCycle) {
  lastError = code;
  faultAddress = addr;
  cycle = currentCycle;
}

// clear all error states
void ExceptionTracker::reset() {
  lastError = ExceptionCode::NONE;
  faultAddress = 0;
  cycle = 0;
}

// check if pc is inside code bounds
ExceptionCode MemorySafetyImpl::checkPC(uint32_t pc) {
  // pc cant go above code limit 0x07FF
  if (pc > 0x07FF) {
    return ExceptionCode::PC_OUT_OF_RANGE;
  }
  return ExceptionCode::NONE;
}

// render ascii stack frame
void renderStackAscii(const uint32_t* memory, uint32_t sp, uint32_t maxDepth) {
  std::cout << "+----------------- STACK -----------------+\n";
  
  // calculate start address to center around sp
  uint32_t startAddr = sp + (maxDepth / 2);
  
  // cant go above initial stack pointer
  if (startAddr > 0x1000) {
    startAddr = 0x1000;
  }
  
  for (uint32_t i = 0; i < maxDepth; i++) {
    if (startAddr == 0) break;
    uint32_t currentAddr = startAddr - 1 - i;
    
    // stop printing if we cross bottom boundary
    if (currentAddr < 0x0C00) break;
    
    uint32_t val = memory[currentAddr];
    
    // draw box with hex and dec values
    std::cout << "| 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << currentAddr << " | ";
    std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << val << " | ";
    std::cout << std::dec << std::setw(10) << std::setfill(' ') << val << " |";
    
    // add pointer marker for sp
    if (currentAddr == sp) {
      std::cout << " <-- SP";
    }
    
    std::cout << "\n";
  }
  
  std::cout << "+-----------------------------------------+\n";
}