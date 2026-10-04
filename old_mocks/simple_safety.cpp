#include "simulator_core/simple_safety.hpp"

namespace risc201 {

ExceptionCode SimpleSafety::checkStack(uint32_t sp, bool isPush) {
  if (isPush) {
    // PUSH and CALL decrement SP first, so the new top has to stay in the stack.
    if (sp <= kStackBase) {
      return ExceptionCode::STACK_OVERFLOW;
    }
    return ExceptionCode::NONE;
  }

  // POP and RET need an item at SP before they increment it.
  if (sp >= kInitialSp) {
    return ExceptionCode::STACK_UNDERFLOW;
  }
  return ExceptionCode::NONE;
}

ExceptionCode SimpleSafety::checkMem(uint32_t addr, bool isWrite) {
  if (addr > kLastAddress) {
    return ExceptionCode::INVALID_MEM_ACCESS;
  }
  if (isWrite && addr <= kCodeLimit) {
    return ExceptionCode::INVALID_MEM_ACCESS;
  }
  return ExceptionCode::NONE;
}

}  // namespace risc201
