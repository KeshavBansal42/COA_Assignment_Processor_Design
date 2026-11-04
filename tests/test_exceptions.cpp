// Exception checks: one test per exception code, and nothing is written back.

#include <cassert>
#include <cstdint>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"
#include "memory_safety/memory_safety.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include "tests/test_programs.hpp"

using namespace risc201;

static void testIllegalInstruction() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 7),
      encodeOpcode(static_cast<Opcode>(0x0E)),  // not a defined opcode
      encodeI(Opcode::LDI, 1, 0, 9),
  });
  cpu.run();

  assert(cpu.exception() == ExceptionCode::ILLEGAL_INSTRUCTION);
  assert(cpu.reg(1) == 7);
  assert(cpu.pc() == kCodeBase + 1);
  assert(!cpu.halted());
}

static void testPcOutOfRange() {
  Cpu cpu;
  cpu.load({encodeOpcode(Opcode::NOP)});  // no HALT
  cpu.run();

  assert(cpu.exception() == ExceptionCode::PC_OUT_OF_RANGE);
  assert(cpu.pc() > kCodeLimit);
}

static void testStackOverflow() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 1),   // 0
      encodeI(Opcode::PUSH, 1, 0, 0),  // 1: loop
      encodeJ(Opcode::JMP, -2),        // 2: back to 1
  });
  cpu.run();

  assert(cpu.exception() == ExceptionCode::STACK_OVERFLOW);
  assert(cpu.sp() == kStackBase);
}

static void testDynamicStackGrowsIntoDataRegion() {
  Cpu cpu;
  cpu.setDynamicStack(true);
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 1),   // 0
      encodeI(Opcode::PUSH, 1, 0, 0),  // 1: loop
      encodeJ(Opcode::JMP, -2),        // 2: back to 1
  });
  cpu.run();

  // With the heap empty the floor is the data base, so the stack may pass the
  // fixed 0xC00 boundary before it overflows.
  assert(cpu.exception() == ExceptionCode::STACK_OVERFLOW);
  assert(cpu.sp() == kDataBase);
  assert(cpu.sp() < kStackBase);
}

static void testStackUnderflow() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::POP, 1, 0, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.exception() == ExceptionCode::STACK_UNDERFLOW);
  assert(cpu.reg(1) == 0);
  assert(cpu.sp() == kInitialSp);
}

static void testLoadOutsideMemory() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, static_cast<int32_t>(kInitialSp)),  // first address past memory
      encodeI(Opcode::LW, 2, 1, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.exception() == ExceptionCode::INVALID_MEM_ACCESS);
  assert(cpu.reg(2) == 0);
}

static void testWriteIntoCodeRegion() {
  Cpu cpu;
  const std::vector<uint32_t> program = {
      encodeI(Opcode::LDI, 1, 0, 0),
      encodeI(Opcode::SW, 1, 1, 0),  // would write mem[0]
      encodeOpcode(Opcode::HALT),
  };
  cpu.load(program);
  cpu.run();

  assert(cpu.exception() == ExceptionCode::INVALID_MEM_ACCESS);
  assert(cpu.readMem(0) == program[0]);
}

static void testDivideByZero() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 5),
      encodeI(Opcode::LDI, 2, 0, 0),
      encodeR(Opcode::DIV, 3, 1, 2),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.exception() == ExceptionCode::DIV_BY_ZERO);
  assert(cpu.reg(3) == 0);
}

static void testResetClearsTheException() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::POP, 1, 0, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();
  assert(cpu.exception() == ExceptionCode::STACK_UNDERFLOW);

  cpu.reset();

  assert(cpu.exception() == ExceptionCode::NONE);
  assert(cpu.sp() == kInitialSp);
  assert(cpu.pc() == kCodeBase);
}

void testExceptionTracker() {
  MemorySafetyImpl safety;
  ExceptionTracker tracker;
  
  uint32_t dummyPc = 0x000;
  uint32_t dummySp = 0x1000;
  uint64_t cycle = 0;
  
  // dummy loop running for 10 cycles
  for (int i = 0; i < 10; i++) {
    cycle++;
    
    // simulate instruction fetch
    ExceptionCode pcCheck = safety.checkPC(dummyPc);
    if (pcCheck != ExceptionCode::NONE) {
      tracker.logException(pcCheck, dummyPc, cycle);
      break;
    }
    
    // simulate a push instruction at cycle 5
    if (cycle == 5) {
      ExceptionCode stackCheck = safety.checkStack(dummySp, true);
      if (stackCheck != ExceptionCode::NONE) {
        tracker.logException(stackCheck, dummySp, cycle);
        break;
      }
      dummySp--; // update sp after push
    }
    
    // simulate bad memory write at cycle 8
    if (cycle == 8) {
      uint32_t badAddr = 0x0100; // writing to code region is bad
      ExceptionCode memCheck = safety.checkMem(badAddr, true);
      if (memCheck != ExceptionCode::NONE) {
        tracker.logException(memCheck, badAddr, cycle);
        break;
      }
    }
    
    dummyPc++;
  }
  
  // check if tracker caught the bad memory write exactly at cycle 8
  assert(tracker.getLastError() == ExceptionCode::INVALID_MEM_ACCESS);
  assert(tracker.getFaultAddress() == 0x0100);
  assert(tracker.getCycle() == 8);
}

void testExceptionToString() {
  // test that exception messages match our specs
  assert(exceptionToString(ExceptionCode::NONE) == "No exception");
  assert(exceptionToString(ExceptionCode::STACK_OVERFLOW) ==
         "Stack overflow (SP crossed the stack floor)");
  assert(exceptionToString(ExceptionCode::STACK_UNDERFLOW) == "Stack underflow (SP went above 0x1000)");
  assert(exceptionToString(ExceptionCode::INVALID_MEM_ACCESS) == "Invalid memory access (out of bounds or code overwrite)");
  assert(exceptionToString(ExceptionCode::DIV_BY_ZERO) == "Division by zero");
  assert(exceptionToString(ExceptionCode::ILLEGAL_INSTRUCTION) == "Illegal instruction encountered");
  assert(exceptionToString(ExceptionCode::PC_OUT_OF_RANGE) == "Program counter went out of code region");
}

void testTrackerFaultAddress() {
  ExceptionTracker tracker;
  
  // manually log an error and check if address is saved correctly
  tracker.logException(ExceptionCode::DIV_BY_ZERO, 0x0500, 42);
  assert(tracker.getLastError() == ExceptionCode::DIV_BY_ZERO);
  assert(tracker.getFaultAddress() == 0x0500);
  assert(tracker.getCycle() == 42);
  
  // check if reset clears everything
  tracker.reset();
  assert(tracker.getLastError() == ExceptionCode::NONE);
  assert(tracker.getFaultAddress() == 0);
  assert(tracker.getCycle() == 0);
}

void exportHexPrograms() {
  // helper lambda to save vector to a text file
  auto saveHex = [](const std::string& name, const std::vector<uint32_t>& prog) {
    std::ofstream file(name);
    for (uint32_t word : prog) {
      file << std::hex << std::setw(8) << std::setfill('0') << word << "\n";
    }
  };
  
  // write our programs to disk
  saveHex("loop.hex", kLoopProgram);
  saveHex("recursion.hex", kRecursionProgram);
  saveHex("stack_fault.hex", kStackOverflowProgram);
  
  std::cout << "exported hex programs for demo!\n";
}

int main() {
  testIllegalInstruction();
  testPcOutOfRange();
  testStackOverflow();
  testDynamicStackGrowsIntoDataRegion();
  testStackUnderflow();
  testLoadOutsideMemory();
  testWriteIntoCodeRegion();
  testDivideByZero();
  testResetClearsTheException();

  testExceptionTracker();
  testExceptionToString();
  testTrackerFaultAddress();

  exportHexPrograms();
  return 0;
}
