// Session level checks: the demo programs from tests/programs, loaded and run
// through the CLI the same way the mid-eval demo does it.

#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>

#include "isa_spec/isa201.h"
#include "simulator_core/cli.hpp"
#include "simulator_core/cpu.hpp"

using namespace risc201;

namespace {

std::string programPath(const std::string& name) {
  return std::string(RISC201_PROGRAMS_DIR) + "/" + name;
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

void testLoopProgram() {
  Cpu cpu;
  const std::string output = runCommands(cpu, "load " + programPath("loop.hex") + "\nrun\n");

  assert(output.find("halted") != std::string::npos);
  assert(cpu.reg(3) == 55);
  assert(cpu.pc() == 7);
}

void testRecursionProgram() {
  Cpu cpu;
  const std::string output =
      runCommands(cpu, "load " + programPath("recursion.hex") + "\nrun\n");

  assert(output.find("halted") != std::string::npos);
  assert(cpu.reg(2) == 10);
  assert(cpu.sp() == kInitialSp);
}

void testStackFaultProgram() {
  Cpu cpu;
  const std::string output =
      runCommands(cpu, "load " + programPath("stack_fault.hex") + "\nrun\n");

  assert(output.find("Stack overflow") != std::string::npos);
  assert(cpu.exception() == ExceptionCode::STACK_OVERFLOW);
  assert(cpu.sp() == kStackBase);
}

// The rehearsal has to run the same demo twice and get the same thing back.
void testDemoRunsCleanlyTwice() {
  Cpu cpu;
  const std::string first =
      runCommands(cpu, "load " + programPath("loop.hex") + "\nrun\nregs\n");
  const uint32_t firstResult = cpu.reg(3);
  const uint32_t firstPc = cpu.pc();

  const std::string second =
      runCommands(cpu, "load " + programPath("loop.hex") + "\nrun\nregs\n");

  assert(firstResult == 55);
  assert(second == first);
  assert(cpu.reg(3) == firstResult);
  assert(cpu.pc() == firstPc);
}

void testDebuggerSessionOnTheLoopProgram() {
  Cpu cpu;
  std::ostringstream out;
  Cli cli(cpu, out);

  // The calls have to stay out of assert(): a release build compiles assert()
  // away, which would drop the calls with it and spin this loop forever.
  const bool loaded = cli.execute("load " + programPath("loop.hex"));
  assert(loaded);

  const bool broke = cli.execute("break 3");
  assert(broke);

  // The breakpoint sits in the loop, so it stops once per pass.
  int stops = 0;
  while (!cpu.halted() && stops < 100) {
    const bool ran = cli.execute("run");
    assert(ran);
    ++stops;
  }

  assert(stops > 1);
  assert(stops < 100);
  assert(cpu.reg(3) == 55);
  assert(out.str().find("breakpoint") != std::string::npos);
}

}  // namespace

int main() {
  testLoopProgram();
  testRecursionProgram();
  testStackFaultProgram();
  testDemoRunsCleanlyTwice();
  testDebuggerSessionOnTheLoopProgram();
  return 0;
}
