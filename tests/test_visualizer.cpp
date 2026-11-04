// Visualizer frame checks: the rendered frame has to show the code line, the
// register values with what changed, the stack and the machine state.

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"
#include "visualizer/view.hpp"

using namespace risc201;

int main() {
  Cpu cpu;
  cpu.load({
      encodeI(Opcode::LDI, 1, 0, 10),
      encodeI(Opcode::LDI, 2, 0, 0),
      encodeI(Opcode::PUSH, 1, 0, 0),
      encodeOpcode(Opcode::HALT),
  });
  cpu.setPipelineMode(PipelineMode::SixStage);

  ViewOptions options;
  options.program = "test.hex";
  options.code = {"LDI R1, 10", "LDI R2, 0", "PUSH R1", "HALT"};
  options.width = 100;

  // Snapshot before the run so the frame can show what changed.
  const MachineSnapshot before = snapshot(cpu);
  cpu.run();
  const std::string frame = renderFrame(cpu, options, &before);

  int problems = 0;
  const auto want = [&](bool present, const char* what) {
    if (!present) {
      ++problems;
      std::cerr << "missing from frame: " << what << "\n";
    }
  };

  want(frame.find("RISC201 visualizer") != std::string::npos, "header");
  want(frame.find("LDI R1, 10") != std::string::npos, "code listing");
  want(frame.find("> ") != std::string::npos, "current line marker");
  want(frame.find("0x0000000A") != std::string::npos, "r1 = 10");
  want(frame.find("<- was 0x00000000") != std::string::npos, "changed register marker");
  want(frame.find("PIPELINE") != std::string::npos, "pipeline panel");
  want(frame.find("STACK") != std::string::npos, "stack panel");
  want(frame.find("SP = 0x00000FFF") != std::string::npos, "stack pointer after the push");
  want(frame.find("0x0FFF  0x0000000A") != std::string::npos, "pushed value on the stack");
  want(frame.find("mode fixed") != std::string::npos, "fixed stack mode");
  want(frame.find("HALTED") != std::string::npos, "halted state");

  // Dynamic mode moves the floor to the heap high-water mark and says so.
  cpu.setDynamicStack(true);
  const std::string dynamic = renderFrame(cpu, options, nullptr);
  want(dynamic.find("stack dynamic") != std::string::npos, "dynamic stack header");
  want(dynamic.find("mode dynamic") != std::string::npos, "dynamic stack mode");
  want(dynamic.find("stack floor 0x0800") != std::string::npos, "dynamic stack floor");
  want(dynamic.find("heap 0 words used") != std::string::npos, "dynamic heap usage");

  std::cout << "visualizer frame checks, problems = " << problems << "\n";
  assert(problems == 0);
  return 0;
}
