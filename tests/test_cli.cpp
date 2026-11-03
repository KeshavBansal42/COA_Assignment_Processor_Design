// Command shell checks, driven through string streams instead of a terminal.

#include <cassert>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "isa_spec/isa201.h"
#include "simulator_core/cli.hpp"
#include "simulator_core/cpu.hpp"

using namespace risc201;

namespace {

const char* kProgramPath = "cli_test_program.hex";

void writeProgramFile() {
  std::ofstream file(kProgramPath);
  file << "# r1 = 42, store r1 at 0x800, then halt\n";
  file << std::hex;
  file << encodeI(Opcode::LDI, 1, 0, 42) << "\n";
  file << encodeI(Opcode::LDI, 2, 0, static_cast<int32_t>(kDataBase)) << "\n";
  file << encodeI(Opcode::SW, 1, 2, 0) << "\n";
  file << encodeOpcode(Opcode::HALT) << "\n";
}

std::string runCommands(Cpu& cpu, const std::string& commands) {
  std::ostringstream out;
  Cli cli(cpu, out);

  std::istringstream input(commands);
  std::string line;
  while (std::getline(input, line)) {
    if (!cli.execute(line)) {
      break;
    }
  }

  return out.str();
}

void testLoadRunAndRegs() {
  writeProgramFile();

  Cpu cpu;
  const std::string output =
      runCommands(cpu, std::string("load ") + kProgramPath + "\nrun\nregs\n");

  assert(output.find("loaded 4 words") != std::string::npos);
  assert(output.find("halted") != std::string::npos);
  assert(output.find("0000002a") != std::string::npos);  // r1 = 42
  assert(cpu.reg(1) == 42);
  assert(cpu.readMem(kDataBase) == 42);
}

void testStepCommand() {
  writeProgramFile();

  Cpu cpu;
  const std::string output =
      runCommands(cpu, std::string("load ") + kProgramPath + "\nstep 2\n");

  assert(cpu.reg(1) == 42);
  assert(!cpu.halted());
  assert(output.find("stopped") != std::string::npos);
}

void testMemCommand() {
  writeProgramFile();

  Cpu cpu;
  const std::string output = runCommands(
      cpu, std::string("load ") + kProgramPath + "\nrun\nmem 0x800 2\nmem 0xfff 4\n");

  assert(output.find("0x00000800: 0000002a 00000000") != std::string::npos);
  assert(output.find("end of memory") != std::string::npos);
}

void testMicroTraceCommand() {
  writeProgramFile();

  Cpu cpu;
  const std::string vertical = runCommands(
      cpu, std::string("load ") + kProgramPath + "\nmicro vertical\nmicrotrace 0\n");

  assert(vertical.find("microtrace PC 0x00000000") != std::string::npos);
  assert(vertical.find("ALU_IMM") != std::string::npos);
  assert(vertical.find("REG_WRITE") != std::string::npos);
  assert(vertical.find("sequencer stop") != std::string::npos);
  assert(vertical.find("decoded signals") != std::string::npos);

  const std::string horizontal =
      runCommands(cpu, "micro horizontal\nmicrotrace 0\n");
  assert(horizontal.find("HORIZONTAL") != std::string::npos);

  // An undefined opcode must be reported, not silently decoded.
  Cpu bad;
  bad.load({0xFC000000u});
  const std::string illegal = runCommands(bad, "microtrace 0\n");
  assert(illegal.find("ILLEGAL_INSTRUCTION") != std::string::npos);
}

void testMissingFile() {
  Cpu cpu;
  const std::string output = runCommands(cpu, "load no_such_program.hex\n");
  assert(output.find("cannot open") != std::string::npos);
}

void testUnknownCommand() {
  Cpu cpu;
  const std::string output = runCommands(cpu, "nonsense\n");
  assert(output.find("unknown command") != std::string::npos);
}

void testQuitEndsTheSession() {
  Cpu cpu;
  std::ostringstream out;
  Cli cli(cpu, out);

  assert(!cli.execute("quit"));
  assert(!cli.execute("exit"));
  assert(cli.execute("regs"));
}

}  // namespace

int main() {
  testLoadRunAndRegs();
  testStepCommand();
  testMemCommand();
  testMicroTraceCommand();
  testMissingFile();
  testUnknownCommand();
  testQuitEndsTheSession();
  
  return 0;
}
