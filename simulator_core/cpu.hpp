#pragma once

// Simulator core.
// The CLI and the debugger only talk to the CPU through these methods.

#include <cstdint>
#include <vector>

#include "isa_spec/isa201.h"

namespace risc201 {

class Cpu {
 public:
  Cpu();

  // Copies a program into code memory and resets the machine first.
  void load(const std::vector<uint32_t>& program);

  // Runs one instruction.
  void step();

  // Runs until HALT or the first exception.
  void run();

  // Puts PC, SP, FLAGS and the registers back to their start values.
  void reset();
};

}  // namespace risc201
