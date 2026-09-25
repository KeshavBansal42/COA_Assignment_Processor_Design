// State, memory and reset checks for the simulator core.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

using namespace risc201;

static void testResetState() {
  Cpu cpu;
  assert(cpu.pc() == kCodeBase);
  assert(cpu.sp() == kInitialSp);
  assert(cpu.flags() == Flags{});
  assert(!cpu.halted());
  for (std::size_t i = 0; i < kRegisterCount; ++i) {
    assert(cpu.reg(static_cast<uint8_t>(i)) == 0);
  }
}

static void testLoadsHandWrittenWords() {
  Cpu cpu;
  const std::vector<uint32_t> program = {
      encodeI(Opcode::LDI, 1, 0, 5),
      encodeR(Opcode::ADD, 2, 1, 1),
      encodeOpcode(Opcode::HALT),
  };
  cpu.load(program);

  for (std::size_t i = 0; i < program.size(); ++i) {
    assert(cpu.readMem(static_cast<uint32_t>(i)) == program[i]);
  }
  assert(cpu.pc() == kCodeBase);
}

static void testStepFetchesAndAdvances() {
  Cpu cpu;
  cpu.load({encodeI(Opcode::LDI, 1, 0, 5)});
  cpu.step();
  assert(cpu.pc() == kCodeBase + 1);
}

static void testResetKeepsTheProgram() {
  Cpu cpu;
  cpu.load({encodeI(Opcode::LDI, 1, 0, 5)});
  cpu.step();
  cpu.setReg(3, 42);
  cpu.setSp(0xFF0);

  cpu.reset();

  assert(cpu.pc() == kCodeBase);
  assert(cpu.sp() == kInitialSp);
  assert(cpu.reg(3) == 0);
  assert(cpu.readMem(0) == encodeI(Opcode::LDI, 1, 0, 5));
}

static void testRegisterAndMemoryWrites() {
  Cpu cpu;
  cpu.setReg(15, 0xDEADBEEFu);
  assert(cpu.reg(15) == 0xDEADBEEFu);

  cpu.writeMem(kDataBase, 1234);
  assert(cpu.readMem(kDataBase) == 1234);

  cpu.setSp(kStackLimit);
  assert(cpu.sp() == kStackLimit);

  cpu.setPc(kCodeBase + 7);
  assert(cpu.pc() == kCodeBase + 7);
}

static void testRunStopsOutsideCode() {
  Cpu cpu;
  cpu.load({encodeOpcode(Opcode::NOP), encodeOpcode(Opcode::NOP)});
  cpu.run();
  assert(cpu.pc() > kCodeLimit);
}

int main() {
  testResetState();
  testLoadsHandWrittenWords();
  testStepFetchesAndAdvances();
  testResetKeepsTheProgram();
  testRegisterAndMemoryWrites();
  testRunStopsOutsideCode();
  return 0;
}
