// Pipeline checks: ADDI, LDI, ADD, SUB, LW, SW and JMP.

#include <cassert>
#include <cstdint>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

using namespace risc201;

static void testLoadImmediateAndAdd() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 5),
      encodeI(Opcode::ADDI, 2, 1, 3),
      encodeR(Opcode::ADD, 3, 1, 2),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(1) == 5);
  assert(cpu.reg(2) == 8);
  assert(cpu.reg(3) == 13);
  assert(cpu.halted());
}

static void testNegativeImmediateAndSub() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 10),
      encodeI(Opcode::ADDI, 2, 1, -4),
      encodeR(Opcode::SUB, 3, 1, 2),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(2) == 6);
  assert(cpu.reg(3) == 4);
  assert(cpu.flags().z == false);
}

static void testSubtractSetsZeroFlag() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 7),
      encodeI(Opcode::LDI, 2, 0, 7),
      encodeR(Opcode::SUB, 3, 1, 2),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(3) == 0);
  assert(cpu.flags().z == true);
  assert(cpu.flags().n == false);
}

static void testLoadAndStore() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 100),
      encodeI(Opcode::LDI, 2, 0, static_cast<int32_t>(kDataBase)),
      encodeI(Opcode::SW, 1, 2, 0),
      encodeI(Opcode::LW, 3, 2, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.readMem(kDataBase) == 100);
  assert(cpu.reg(3) == 100);
}

static void testJumpSkipsInstructions() {
  Cpu cpu;
  cpu.load({
      encodeJ(Opcode::JMP, 2),
      encodeI(Opcode::LDI, 1, 0, 111),
      encodeI(Opcode::LDI, 1, 0, 222),
      encodeI(Opcode::LDI, 1, 0, 333),
      encodeOpcode(Opcode::HALT),
  });
  cpu.run();

  assert(cpu.reg(1) == 333);
  assert(cpu.halted());
}

static void testStepRunsOneInstruction() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 5),
      encodeOpcode(Opcode::HALT),
  });

  cpu.step();
  assert(cpu.reg(1) == 5);
  assert(!cpu.halted());

  cpu.step();
  assert(cpu.halted());

  cpu.step();
  assert(cpu.halted());
}

int main() {
  testLoadImmediateAndAdd();
  testNegativeImmediateAndSub();
  testSubtractSetsZeroFlag();
  testLoadAndStore();
  testJumpSkipsInstructions();
  testStepRunsOneInstruction();
  return 0;
}
