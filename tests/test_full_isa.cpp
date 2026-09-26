// Full ISA checks: logic and shift ops, mul/div, branches, stack and CALL/RET.

#include <cassert>
#include <cstdint>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

using namespace risc201;

static void testLogicAndShiftOps() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 12),   // 0b1100
      encodeI(Opcode::LDI, 2, 0, 10),   // 0b1010
      encodeI(Opcode::LDI, 3, 0, 2),
      encodeR(Opcode::AND, 4, 1, 2),
      encodeR(Opcode::OR, 5, 1, 2),
      encodeR(Opcode::XOR, 6, 1, 2),
      encodeR(Opcode::NOT, 7, 1, 0),
      encodeR(Opcode::SHL, 8, 1, 3),
      encodeR(Opcode::SHR, 9, 8, 3),
      encodeR(Opcode::MOV, 10, 1, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(4) == 8);
  assert(cpu.reg(5) == 14);
  assert(cpu.reg(6) == 6);
  assert(cpu.reg(7) == 0xFFFFFFF3u);
  assert(cpu.reg(8) == 48);
  assert(cpu.reg(9) == 12);
  assert(cpu.reg(10) == 12);
}

static void testMulAndDiv() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 6),
      encodeI(Opcode::LDI, 2, 0, 7),
      encodeR(Opcode::MUL, 3, 1, 2),
      encodeR(Opcode::DIV, 4, 3, 2),
      encodeR(Opcode::DIV, 5, 2, 1),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(3) == 42);
  assert(cpu.reg(4) == 6);
  assert(cpu.reg(5) == 1);
}

static void testCountdownLoop() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 0),    // 0: counter
      encodeI(Opcode::LDI, 2, 0, 5),    // 1: limit
      encodeI(Opcode::LDI, 3, 0, 0),    // 2: zero, for CMP
      encodeI(Opcode::ADDI, 1, 1, 1),   // 3: loop
      encodeI(Opcode::ADDI, 2, 2, -1),  // 4
      encodeR(Opcode::CMP, 0, 2, 3),    // 5: CMP r2, r3
      encodeJ(Opcode::BNE, -4),         // 6: back to 3
      encodeOpcode(Opcode::HALT),       // 7
  });
  cpu.run();

  assert(cpu.reg(1) == 5);
  assert(cpu.reg(2) == 0);
  assert(cpu.halted());
}

static void testBranchLessThan() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 3),    // 0
      encodeI(Opcode::LDI, 2, 0, 10),   // 1
      encodeR(Opcode::CMP, 0, 1, 2),    // 2: CMP r1, r2, so 3 - 10 is negative
      encodeJ(Opcode::BLT, 2),          // 3: to 6
      encodeI(Opcode::LDI, 3, 0, 111),  // 4: skipped
      encodeI(Opcode::LDI, 3, 0, 222),  // 5: skipped
      encodeI(Opcode::LDI, 3, 0, 333),  // 6
      encodeOpcode(Opcode::HALT),       // 7
  });
  cpu.run();

  assert(cpu.reg(3) == 333);
}

static void testBranchGreaterEqual() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 10),   // 0
      encodeI(Opcode::LDI, 2, 0, 3),    // 1
      encodeR(Opcode::CMP, 0, 1, 2),    // 2: CMP r1, r2, so 10 - 3 is positive
      encodeJ(Opcode::BGE, 2),          // 3: to 6
      encodeI(Opcode::LDI, 3, 0, 111),  // 4: skipped
      encodeI(Opcode::LDI, 3, 0, 222),  // 5: skipped
      encodeI(Opcode::LDI, 3, 0, 333),  // 6
      encodeOpcode(Opcode::HALT),       // 7
  });
  cpu.run();

  assert(cpu.reg(3) == 333);
}

static void testPushAndPop() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 42),
      encodeI(Opcode::LDI, 2, 0, 99),
      encodeI(Opcode::PUSH, 1, 0, 0),
      encodeI(Opcode::PUSH, 2, 0, 0),
      encodeI(Opcode::POP, 3, 0, 0),
      encodeI(Opcode::POP, 4, 0, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(3) == 99);
  assert(cpu.reg(4) == 42);
  assert(cpu.sp() == kInitialSp);
}

static void testCallAndReturn() {
  Cpu cpu;
  cpu.load({
      encodeJ(Opcode::CALL, 1),         // 0: to 2, pushes 1
      encodeOpcode(Opcode::HALT),       // 1
      encodeI(Opcode::LDI, 1, 0, 7),    // 2
      encodeI(Opcode::RET, 0, 0, 0),    // 3
  });
  cpu.run();

  assert(cpu.reg(1) == 7);
  assert(cpu.sp() == kInitialSp);
  assert(cpu.halted());
}

static void testRecursiveSum() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 4),    // 0: n
      encodeI(Opcode::LDI, 3, 0, 0),    // 1: zero
      encodeJ(Opcode::CALL, 1),         // 2: to 4, pushes 3
      encodeOpcode(Opcode::HALT),       // 3
      encodeR(Opcode::CMP, 0, 1, 3),    // 4: sum: CMP r1, r3, is n == 0?
      encodeJ(Opcode::BEQ, 6),          // 5: to 12
      encodeI(Opcode::PUSH, 1, 0, 0),   // 6: save n
      encodeI(Opcode::ADDI, 1, 1, -1),  // 7: n - 1
      encodeJ(Opcode::CALL, -5),        // 8: recurse, pushes 9
      encodeI(Opcode::POP, 4, 0, 0),    // 9: restore n
      encodeR(Opcode::ADD, 2, 4, 2),    // 10: n + sum(n - 1)
      encodeI(Opcode::RET, 0, 0, 0),    // 11
      encodeI(Opcode::LDI, 2, 0, 0),    // 12: base case
      encodeI(Opcode::RET, 0, 0, 0),    // 13
  });
  cpu.run();

  assert(cpu.reg(2) == 10);
  assert(cpu.sp() == kInitialSp);
  assert(cpu.halted());
}

int main() {
  testLogicAndShiftOps();
  testMulAndDiv();
  testCountdownLoop();
  testBranchLessThan();
  testBranchGreaterEqual();
  testPushAndPop();
  testCallAndReturn();
  testRecursiveSum();
  return 0;
}
