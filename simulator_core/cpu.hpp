#pragma once

// Simulator core: the machine state and the run loop.
// The CLI and the debugger only touch the CPU through this class.

#include <array>
#include <cstdint>
#include <vector>

#include "isa_spec/isa201.h"

namespace risc201 {

class Cpu {
 public:
  Cpu();

  // Clears memory, copies the program into the code region and resets the state.
  void load(const std::vector<uint32_t>& program);

  // Runs one instruction.
  void step();

  // Runs until HALT or until PC leaves the code region.
  void run();

  // Puts the registers, PC, SP, FLAGS and the halt flag back to their start values.
  // Memory is kept so the loaded program survives a reset.
  void reset();

  uint32_t reg(uint8_t index) const;
  void setReg(uint8_t index, uint32_t value);

  uint32_t pc() const;
  void setPc(uint32_t value);

  uint32_t sp() const;
  void setSp(uint32_t value);

  Flags flags() const;
  void setFlags(const Flags& value);

  // Word addressed memory, one 32-bit word per address.
  uint32_t readMem(uint32_t address) const;
  void writeMem(uint32_t address, uint32_t value);

  bool halted() const;

 private:
  std::array<uint32_t, kRegisterCount> registers_{};
  std::array<uint32_t, kMemoryWords> memory_{};
  uint32_t pc_ = kCodeBase;
  uint32_t sp_ = kInitialSp;
  Flags flags_{};
  bool halted_ = false;
};

}  // namespace risc201
