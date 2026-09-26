#include "memory_safety/memory_safety.h"

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



}  // namespace risc201