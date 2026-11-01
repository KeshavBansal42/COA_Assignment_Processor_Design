// Control unit checks: the horizontal ROM and the vertical microprogram have to
// decode every instruction to the same control signals, and the mode switch has
// to actually change which encoding is read.

#include <cassert>
#include <cstdint>
#include <iostream>

#include "control_unit/control_unit.hpp"
#include "control_unit/vertical_microcode.hpp"
#include "isa_spec/isa201.h"

using namespace risc201;

namespace {

// ControlSignals has no operator==, so compare the fields by hand.
bool sameSignals(const ControlSignals& a, const ControlSignals& b) {
  return a.opcode == b.opcode && a.aluOp == b.aluOp && a.aluSrc == b.aluSrc &&
         a.resultSrc == b.resultSrc && a.regWrite == b.regWrite &&
         a.memRead == b.memRead && a.memWrite == b.memWrite &&
         a.branch == b.branch && a.jump == b.jump && a.push == b.push &&
         a.pop == b.pop && a.ret == b.ret && a.halt == b.halt &&
         a.writeFlags == b.writeFlags && a.illegal == b.illegal;
}

}  // namespace

int main() {
  ControlUnit horizontal;                       // default mode
  ControlUnit vertical;
  vertical.setMode(ControlMode::Vertical);

  assert(horizontal.mode() == ControlMode::Horizontal);
  assert(vertical.mode() == ControlMode::Vertical);

  // Every defined opcode must decode the same way in both encodings.
  int mismatches = 0;
  for (std::size_t i = 0; i < kInstructionCount; ++i) {
    const Opcode opcode = kInstructionTable[i].opcode;
    const uint32_t word = encodeOpcode(opcode);

    const ControlSignals fromRom = horizontal.decode(word);
    const ControlSignals fromMicro = vertical.decode(word);

    if (!sameSignals(fromRom, fromMicro)) {
      ++mismatches;
      std::cerr << "mismatch on " << kInstructionTable[i].mnemonic << "\n";
    }
  }
  std::cout << "compared " << kInstructionCount << " opcodes, mismatches = "
            << mismatches << "\n";
  assert(mismatches == 0);

  // An undefined opcode is illegal in both encodings.
  const uint32_t undefinedWord = encodeOpcode(static_cast<Opcode>(0x3F));
  assert(horizontal.decode(undefinedWord).illegal);
  assert(vertical.decode(undefinedWord).illegal);

  // The vertical sequencer should report a useful micro pc and micro op count.
  const ControlSignals addSignals = vertical.decode(encodeOpcode(Opcode::ADD));
  assert(vertical.getMicroPC() == kMicroProgram.start[static_cast<std::size_t>(Opcode::ADD)]);
  assert(vertical.lastMicroOpCount() >= 2);
  assert(addSignals.regWrite && addSignals.writeFlags && addSignals.aluOp == AluOp::ADD);

  // Horizontal reads one wide word, so it always counts a single micro op.
  horizontal.decode(encodeOpcode(Opcode::ADD));
  assert(horizontal.lastMicroOpCount() == 1);

  // Switching modes mid-session has to take effect on the next decode.
  horizontal.setMode(ControlMode::Vertical);
  const ControlSignals afterSwitch = horizontal.decode(encodeOpcode(Opcode::LW));
  assert(afterSwitch.memRead && afterSwitch.resultSrc == ResultSrc::Memory);

  std::cout << "control unit horizontal/vertical checks passed\n";
  return 0;
}
