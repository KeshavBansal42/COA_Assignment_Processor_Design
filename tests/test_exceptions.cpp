// Exception checks: one test per exception code, and nothing is written back.

#include <cassert>
#include <cstdint>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

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

int main() {
  testIllegalInstruction();
  testPcOutOfRange();
  testStackOverflow();
  testStackUnderflow();
  testLoadOutsideMemory();
  testWriteIntoCodeRegion();
  testDivideByZero();
  testResetClearsTheException();
  return 0;
}
