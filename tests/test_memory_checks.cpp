#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
#include "memory_safety/memory_safety.hpp"

using namespace risc201;

static void testStaticMemorySafety() {
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

  // check stack operations (default fixed 0xC00 floor)
  // push at top is fine
  assert(safety.checkStack(0x1000, true) == ExceptionCode::NONE);
  // pop at top is underflow error
  assert(safety.checkStack(0x1000, false) == ExceptionCode::STACK_UNDERFLOW);

  // push at bottom boundary gives overflow in fixed mode
  assert(safety.checkStack(0xC00, true) == ExceptionCode::STACK_OVERFLOW);
  // pop at bottom boundary is ok
  assert(safety.checkStack(0xC00, false) == ExceptionCode::NONE);

  // way below stack limit
  assert(safety.checkStack(0xBF1, true) == ExceptionCode::STACK_OVERFLOW);
  assert(safety.checkStack(0xBF1, false) == ExceptionCode::NONE);
}

static void testDynamicStackAndHeapBoundary() {
  MemorySafetyImpl safety;

  // By default, dynamic mode is disabled and heapTop is 0xC00
  assert(!safety.isDynamicStackEnabled());
  assert(safety.heapTop() == 0xC00);

  // Enable dynamic stack mode with empty heap (base = 0x800)
  safety.enableDynamicStack(true);
  assert(safety.isDynamicStackEnabled());
  assert(safety.heapTop() == 0x800);

  // In dynamic mode, stack can push past 0xC00!
  assert(safety.checkStack(0xC00, true) == ExceptionCode::NONE);
  assert(safety.checkStack(0x900, true) == ExceptionCode::NONE);
  assert(safety.checkStack(0x801, true) == ExceptionCode::NONE);

  // But pushing below heapTop (0x800) triggers STACK_OVERFLOW
  assert(safety.checkStack(0x800, true) == ExceptionCode::STACK_OVERFLOW);

  // Test dynamic heap allocation
  uint32_t currentSp = 0x900;
  // Allocate 100 words on heap: goes from 0x800 to 0x864
  uint32_t block1 = safety.allocHeap(100, currentSp);
  assert(block1 == 0x800);
  assert(safety.heapSize() == 100);
  assert(safety.heapTop() == 0x864);

  // Stack can still push above 0x864
  assert(safety.checkStack(0x865, true) == ExceptionCode::NONE);
  // Pushing below 0x864 now overflows because heap occupies up to 0x864!
  assert(safety.checkStack(0x864, true) == ExceptionCode::STACK_OVERFLOW);

  // Test collision detection: heap cannot grow into current SP (0x900)
  // Requesting 200 words from 0x864 would reach 0x92C >= 0x900 -> MUST FAIL
  uint32_t badBlock = safety.allocHeap(200, currentSp);
  assert(badBlock == 0); // collision detected!
  assert(safety.heapTop() == 0x864); // unchanged

  // Sizing metrics
  assert(safety.stackDepth(currentSp) == (0x1000 - 0x900));
  assert(safety.availableFreeWords(currentSp) == (currentSp - safety.heapTop()));
  assert(safety.stackCapacity(currentSp) == (currentSp - safety.heapTop()));

  // Free heap words
  safety.freeHeap(50);
  assert(safety.heapSize() == 50);
  assert(safety.heapTop() == 0x832);

  // Disable dynamic mode -> reverts stack floor to fixed 0xC00
  safety.enableDynamicStack(false);
  assert(!safety.isDynamicStackEnabled());
  assert(safety.heapTop() == 0xC00);
  assert(safety.checkStack(0xC00, true) == ExceptionCode::STACK_OVERFLOW);
}

static void testStreamRenderStackAscii() {
  std::vector<uint32_t> memory(kMemoryWords, 0);
  // Setup dummy stack contents
  memory[0xFFF] = 0x12345678;
  memory[0xFFE] = 0x00000042; // looks like return address in code region

  std::ostringstream ss;
  renderStackAscii(ss, memory.data(), 0xFFE, 8);
  std::string output = ss.str();

  assert(!output.empty());
  assert(output.find("STACK") != std::string::npos);
  assert(output.find("<-- SP") != std::string::npos);
  assert(output.find("FRAME") != std::string::npos);
}

static void testExceptionTrackerHistory() {
  ExceptionTracker tracker;
  assert(tracker.history().empty());

  tracker.logException(ExceptionCode::STACK_OVERFLOW, 0x800, 100);
  tracker.logException(ExceptionCode::INVALID_MEM_ACCESS, 0x100, 250);

  assert(tracker.getLastError() == ExceptionCode::INVALID_MEM_ACCESS);
  assert(tracker.getFaultAddress() == 0x100);
  assert(tracker.getCycle() == 250);

  assert(tracker.history().size() == 2);
  assert(tracker.history()[0].code == ExceptionCode::STACK_OVERFLOW);
  assert(tracker.history()[0].cycle == 100);
  assert(tracker.history()[1].code == ExceptionCode::INVALID_MEM_ACCESS);
  assert(tracker.history()[1].cycle == 250);

  tracker.reset();
  assert(tracker.getLastError() == ExceptionCode::NONE);
  assert(tracker.history().empty());
}

int main() {
  testStaticMemorySafety();
  testDynamicStackAndHeapBoundary();
  testStreamRenderStackAscii();
  testExceptionTrackerHistory();

  std::cout << "all mem safety tests passed (static, dynamic, stream, and tracker)\n";
  return 0;
}
