#pragma once

// Renders one frame of the visualizer: the code listing with the current line
// marked, the registers and flags with what changed, the stack with the stack
// pointer, the pipeline occupancy and the controls line.

#include <cstdint>
#include <string>
#include <vector>

#include "simulator_core/cpu.hpp"

namespace risc201 {

struct ViewOptions {
  std::string program;             // program path, shown in the header
  std::vector<std::string> code;   // one disassembled line per address
  std::string status;              // a short message under the panels
  bool playing = false;
  int speedMs = 100;
  int width = 100;
};

// A snapshot of the machine, used to highlight what changed between frames.
struct MachineSnapshot {
  std::vector<uint32_t> regs;
  uint32_t pc = 0;
  uint32_t sp = 0;
  Flags flags{};
  std::vector<uint32_t> stack;
};

MachineSnapshot snapshot(const Cpu& cpu);

// previous may be null on the first frame.
std::string renderFrame(const Cpu& cpu, const ViewOptions& options,
                        const MachineSnapshot* previous);

}  // namespace risc201
