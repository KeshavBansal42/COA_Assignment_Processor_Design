#include "memory_safety/memory_safety.h"
#include <string>

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