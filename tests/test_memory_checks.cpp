#include <cassert>
#include <iostream>
#include "memory_safety/memory_safety.h"

using namespace risc201;

int main() {
  MemorySafetyImpl safety;

  // check mem read and writes
  // reading code region is ok
  assert(safety.checkMem(0x000, false) == ExceptionCode::NONE);
  assert(safety.checkMem(0x7FF, false) == ExceptionCode::NONE);
  
  // writing to code region should block
  assert(safety.checkMem(0x000, true) == ExceptionCode::INVALID_MEM_ACCESS);
  assert(safety.checkMem(0x7FF, true) == ExceptionCode::INVALID_MEM_ACCESS);

  // data and stack region both read write allowed
  assert(safety.checkMem(0x800, false) == ExceptionCode::NONE);
  assert(safety.checkMem(0x800, true) == ExceptionCode::NONE);
  assert(safety.checkMem(0xFFF, false) == ExceptionCode::NONE);
  assert(safety.checkMem(0xFFF, true) == ExceptionCode::NONE);

  // over memory limit 
  assert(safety.checkMem(0x1000, false) == ExceptionCode::INVALID_MEM_ACCESS);
  assert(safety.checkMem(0x1000, true) == ExceptionCode::INVALID_MEM_ACCESS);

  // check stack operations
  // push at top is fine
  assert(safety.checkStack(0x1000, true) == ExceptionCode::NONE);
  // pop at top is underflow error
  assert(safety.checkStack(0x1000, false) == ExceptionCode::STACK_UNDERFLOW);

  // push at bottom boundary gives overflow
  assert(safety.checkStack(0xC00, true) == ExceptionCode::STACK_OVERFLOW);
  // pop at bottom boundary is ok
  assert(safety.checkStack(0xC00, false) == ExceptionCode::NONE);

  // way below stack limit
  assert(safety.checkStack(0xBF1, true) == ExceptionCode::STACK_OVERFLOW);
  // pop here wont give underflow (checkMem handles invalid access later)
  assert(safety.checkStack(0xBF1, false) == ExceptionCode::NONE);

  std::cout << "all mem safety tests passed\n";
  return 0;
}