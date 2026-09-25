// Debugger checks: breakpoints, watchpoints, symbols and reset.

#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"
#include "simulator_core/debugger.hpp"

using namespace risc201;

namespace {

std::vector<uint32_t> testProgram() {
  return {
      encodeI(Opcode::LDI, 1, 0, 5),     // 0
      encodeI(Opcode::LDI, 2, 0, 6),     // 1
      encodeR(Opcode::ADD, 3, 1, 2),     // 2
      encodeOpcode(Opcode::HALT),        // 3
  };
}

std::vector<uint32_t> storeProgram() {
  return {
      encodeI(Opcode::LDI, 1, 0, 42),                       // 0
      encodeI(Opcode::LDI, 2, 0, static_cast<int32_t>(kDataBase)),  // 1
      encodeI(Opcode::SW, 1, 2, 0),                         // 2
      encodeI(Opcode::LDI, 4, 0, 7),                        // 3
      encodeOpcode(Opcode::HALT),                           // 4
  };
}

void testBreakpointStopsBeforeTheInstruction() {
  Cpu cpu;
  cpu.load(testProgram());
  Debugger debugger(cpu);
  debugger.addBreakpoint(2);

  assert(debugger.continueRun() == StopReason::Breakpoint);
  assert(debugger.stopAddress() == 2);
  assert(cpu.pc() == 2);
  assert(cpu.reg(1) == 5);
  assert(cpu.reg(2) == 6);
  assert(cpu.reg(3) == 0);  // the breakpoint is before the ADD

  // Continuing from a breakpoint runs that instruction instead of trapping again.
  assert(debugger.continueRun() == StopReason::Halted);
  assert(cpu.reg(3) == 11);
  assert(cpu.halted());
}

void testContinueWithoutBreakpoints() {
  Cpu cpu;
  cpu.load(testProgram());
  Debugger debugger(cpu);

  assert(debugger.continueRun() == StopReason::Halted);
  assert(cpu.reg(3) == 11);
}

void testWatchpointStopsAfterTheStore() {
  Cpu cpu;
  cpu.load(storeProgram());
  Debugger debugger(cpu);
  debugger.addWatchpoint(kDataBase);

  assert(debugger.continueRun() == StopReason::Watchpoint);
  assert(debugger.stopAddress() == kDataBase);
  assert(cpu.readMem(kDataBase) == 42);
  assert(cpu.pc() == 3);  // the store finished, the next instruction did not start

  assert(debugger.continueRun() == StopReason::Halted);
}

void testStepOnce() {
  Cpu cpu;
  cpu.load(testProgram());
  Debugger debugger(cpu);

  assert(debugger.stepOnce() == StopReason::Running);
  assert(cpu.reg(1) == 5);
  assert(debugger.stopAddress() == 1);
}

void testBreakpointsAndWatchpointsAreUnique() {
  Cpu cpu;
  Debugger debugger(cpu);

  assert(!debugger.hasBreakpoint(3));
  debugger.addBreakpoint(3);
  debugger.addBreakpoint(3);
  assert(debugger.breakpoints().size() == 1);
  assert(debugger.removeBreakpoint(3));
  assert(!debugger.removeBreakpoint(3));

  debugger.addWatchpoint(kDataBase);
  debugger.addWatchpoint(kDataBase);
  assert(debugger.watchpoints().size() == 1);
  assert(debugger.hasWatchpoint(kDataBase));
  assert(debugger.removeWatchpoint(kDataBase));
  assert(!debugger.removeWatchpoint(kDataBase));
}

void testSymbolsAndBreakByLabel() {
  const std::string path = "debugger_test_symbols.sym";
  {
    std::ofstream file(path);
    file << "# labels\n";
    file << "start 0\n";
    file << "loop = 2\n";
  }

  Cpu cpu;
  cpu.load(testProgram());
  Debugger debugger(cpu);

  std::string error;
  assert(debugger.loadSymbols(path, error));

  uint32_t address = 0;
  assert(debugger.findSymbol("loop", address));
  assert(address == 2);
  assert(!debugger.findSymbol("missing", address));
  assert(debugger.symbolFor(0) == "start");

  debugger.addBreakpoint(address);
  assert(debugger.continueRun() == StopReason::Breakpoint);
  assert(cpu.pc() == 2);
}

void testBadSymbolFile() {
  const std::string path = "debugger_test_bad.sym";
  {
    std::ofstream file(path);
    file << "loop\n";
  }

  Cpu cpu;
  Debugger debugger(cpu);
  std::string error;
  assert(!debugger.loadSymbols(path, error));
  assert(!error.empty());

  assert(!debugger.loadSymbols("no_such_symbols.sym", error));
}

void testResetKeepsTheProgram() {
  Cpu cpu;
  cpu.load(testProgram());
  Debugger debugger(cpu);
  debugger.continueRun();
  assert(cpu.halted());

  debugger.reset();

  assert(!cpu.halted());
  assert(cpu.reg(1) == 0);
  assert(cpu.pc() == kCodeBase);
  assert(cpu.readMem(0) == testProgram()[0]);
  assert(debugger.stopReason() == StopReason::Running);
}

}  // namespace

int main() {
  testBreakpointStopsBeforeTheInstruction();
  testContinueWithoutBreakpoints();
  testWatchpointStopsAfterTheStore();
  testStepOnce();
  testBreakpointsAndWatchpointsAreUnique();
  testSymbolsAndBreakByLabel();
  testBadSymbolFile();
  testResetKeepsTheProgram();
  return 0;
}
