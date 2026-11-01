#pragma once

// The control unit turns one instruction word into the control signals the
// pipeline reads. It can read them from either encoding:
//   Horizontal: one wide ROM word per instruction, one bit per control wire.
//   Vertical:   narrow encoded micro operations walked by a sequencer.

#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

// Which control memory the decode reads from.
enum class ControlMode {
  Horizontal,
  Vertical,
};

class ControlUnit {
 public:
  // Turns one instruction word into the signals the pipeline needs.
  ControlSignals decode(uint32_t instr);

  // Which word of control memory the current instruction is on.
  uint32_t getMicroPC() const;

  // How many micro operations the last decode walked. One for horizontal.
  uint32_t lastMicroOpCount() const;

  void setMode(ControlMode mode);
  ControlMode mode() const;

 private:
  ControlSignals decodeHorizontal(uint32_t instr);
  ControlSignals decodeVertical(uint32_t instr);

  ControlMode mode_ = ControlMode::Horizontal;

  // keeps track of what rom address we are on
  uint32_t microPc_ = 0;
  uint32_t microOpCount_ = 0;
};

// alias so the cpu and the existing tests keep compiling
using MicroprogrammedControl = ControlUnit;

}  // namespace risc201
