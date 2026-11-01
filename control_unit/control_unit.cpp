#include "control_unit/control_unit.hpp"

#include "control_unit/control_rom.hpp"
#include "control_unit/vertical_microcode.hpp"

namespace risc201 {

namespace {

// proper risc201 shift to get the opcode, which addresses control memory
uint8_t extractOpcode(uint32_t instr) {
  return static_cast<uint8_t>((instr >> 26) & 0x3F);
}

}  // namespace

ControlSignals ControlUnit::decode(uint32_t instr) {
  if (mode_ == ControlMode::Vertical) {
    return decodeVertical(instr);
  }
  return decodeHorizontal(instr);
}

// decode by reading one wide word from the horizontal ROM
ControlSignals ControlUnit::decodeHorizontal(uint32_t instr) {
  ControlSignals signals;

  // extract opcode to use as the ROM memory address
  uint8_t op = extractOpcode(instr);
  signals.opcode = static_cast<Opcode>(op);

  // fetch the 32-bit microinstruction from our simulated ROM chip
  uint32_t microword = CONTROL_ROM[op];

  // update micro pc for the debugger (we are horizontal so its just 1 step)
  microPc_ = op;
  microOpCount_ = 1;

  // unpack the physical wires back into the pipeline control struct
  using namespace MicrocodeBits;
  signals.regWrite   = (microword & REG_WRITE) != 0;
  signals.memRead    = (microword & MEM_READ) != 0;
  signals.memWrite   = (microword & MEM_WRITE) != 0;
  signals.branch     = (microword & BRANCH) != 0;
  signals.jump       = (microword & JUMP) != 0;
  signals.push       = (microword & PUSH) != 0;
  signals.pop        = (microword & POP) != 0;
  signals.ret        = (microword & RET) != 0;
  signals.halt       = (microword & HALT) != 0;
  signals.writeFlags = (microword & WRITE_FLAGS) != 0;
  signals.illegal    = (microword & ILLEGAL) != 0;

  // mux selectors
  signals.aluSrc     = (microword & ALU_SRC_IMM) ? AluSrc::Immediate : AluSrc::Register;
  signals.resultSrc  = (microword & RES_SRC_MEM) ? ResultSrc::Memory : ResultSrc::Alu;

  // extract the 4-bit ALU operation enum
  uint32_t aluOpInt  = (microword >> ALU_OP_SHIFT) & ALU_OP_MASK;
  signals.aluOp      = static_cast<AluOp>(aluOpInt);

  return signals;
}

// walk the vertical microprogram for one instruction and fold every micro
// operation into the same signals the horizontal decode produces
ControlSignals ControlUnit::decodeVertical(uint32_t instr) {
  ControlSignals signals;

  const uint8_t op = extractOpcode(instr);
  signals.opcode = static_cast<Opcode>(op);

  const uint16_t start = kMicroProgram.start[op];

  // no microprogram for this opcode means it is an illegal instruction
  if (start == kNoMicroProgram) {
    signals.illegal = true;
    microPc_ = 0;
    microOpCount_ = 0;
    return signals;
  }

  microPc_ = start;

  uint32_t count = 0;
  std::size_t index = start;
  for (;;) {
    const uint16_t microword = kMicroProgram.words[index];
    const MicroOp micro = microOpOf(microword);

    if (micro == MicroOp::END) {
      break;
    }
    ++count;

    switch (micro) {
      case MicroOp::ALU_REG:
        signals.aluSrc = AluSrc::Register;
        signals.aluOp = microAluOf(microword);
        break;
      case MicroOp::ALU_IMM:
        signals.aluSrc = AluSrc::Immediate;
        signals.aluOp = microAluOf(microword);
        break;
      case MicroOp::REG_WRITE:
        signals.regWrite = true;
        break;
      case MicroOp::FLAGS:
        signals.writeFlags = true;
        break;
      case MicroOp::LOAD:
        signals.memRead = true;
        signals.resultSrc = ResultSrc::Memory;
        signals.regWrite = true;
        break;
      case MicroOp::STORE:
        signals.memWrite = true;
        break;
      case MicroOp::PUSH:
        signals.push = true;
        signals.memWrite = true;
        break;
      case MicroOp::POP:
        signals.pop = true;
        signals.memRead = true;
        signals.resultSrc = ResultSrc::Memory;
        signals.regWrite = true;
        break;
      case MicroOp::RET:
        signals.ret = true;
        signals.memRead = true;
        break;
      case MicroOp::JUMP:
        signals.jump = true;
        break;
      case MicroOp::BRANCH:
        signals.branch = true;
        break;
      case MicroOp::HALT:
        signals.halt = true;
        break;
      case MicroOp::END:
        break;
    }

    ++index;
  }

  microOpCount_ = count;
  return signals;
}

uint32_t ControlUnit::getMicroPC() const { return microPc_; }

uint32_t ControlUnit::lastMicroOpCount() const { return microOpCount_; }

void ControlUnit::setMode(ControlMode mode) { mode_ = mode; }

ControlMode ControlUnit::mode() const { return mode_; }

}  // namespace risc201
