#pragma once

// Simulator core: the machine state and the run loop.
// The CLI and the debugger only touch the CPU through this class.

#include <array>
#include <cstdint>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/hardwired_control.hpp"
#include "simulator_core/pipeline.hpp"
#include "simulator_core/simple_alu.hpp"
#include "simulator_core/simple_safety.hpp"

namespace risc201 {

class Cpu {
 public:
  Cpu();

  // Clears memory, copies the program into the code region and resets the state.
  void load(const std::vector<uint32_t>& program);

  // Runs one instruction.
  void step();

  // Runs until HALT, an exception or the end of the code region.
  void run();

  // Puts the registers, PC, SP, FLAGS, the halt flag and the exception back to
  // their start values. Memory is kept so the loaded program survives a reset.
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
  void setHalted(bool value);

  // NONE until an instruction raises an exception, which stops the machine.
  ExceptionCode exception() const;
  void setException(ExceptionCode value);

  // True when the last step wrote memory, with the address it wrote to.
  bool wroteMemory() const;
  uint32_t lastWriteAddress() const;

 private:
  std::array<uint32_t, kRegisterCount> registers_{};
  std::array<uint32_t, kMemoryWords> memory_{};
  uint32_t pc_ = kCodeBase;
  uint32_t sp_ = kInitialSp;
  Flags flags_{};
  bool halted_ = false;
  ExceptionCode exception_ = ExceptionCode::NONE;
  bool wroteMemory_ = false;
  uint32_t lastWriteAddress_ = 0;

  // Stand-ins until the real control unit, ALU and safety modules are ready.
  HardwiredControl control_{};
  SimpleAlu alu_{};
  SimpleSafety safety_{};
  Pipeline pipeline_;
};

}  // namespace risc201
