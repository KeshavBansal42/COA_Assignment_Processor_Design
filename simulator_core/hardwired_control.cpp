#include "simulator_core/hardwired_control.hpp"

namespace risc201 {

ControlSignals HardwiredControl::decode(uint32_t instr) {
  ControlSignals signals;
  signals.opcode = decodeOpcode(instr);

  switch (signals.opcode) {
    case Opcode::NOP:
      break;

    case Opcode::HALT:
      signals.halt = true;
      break;

    case Opcode::ADD:
      signals.aluOp = AluOp::ADD;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::SUB:
      signals.aluOp = AluOp::SUB;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::ADDI:
      signals.aluOp = AluOp::ADD;
      signals.aluSrc = AluSrc::Immediate;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::LDI:
      signals.aluOp = AluOp::ADD;
      signals.aluSrc = AluSrc::Immediate;
      signals.regWrite = true;
      break;

    case Opcode::LW:
      signals.aluOp = AluOp::ADD;
      signals.aluSrc = AluSrc::Immediate;
      signals.memRead = true;
      signals.resultSrc = ResultSrc::Memory;
      signals.regWrite = true;
      break;

    case Opcode::SW:
      signals.aluOp = AluOp::ADD;
      signals.aluSrc = AluSrc::Immediate;
      signals.memWrite = true;
      break;

    case Opcode::JMP:
      signals.jump = true;
      break;

    default:
      // Opcodes that are not decoded yet, they are handled as they land.
      signals.illegal = true;
      break;
  }

  return signals;
}

uint32_t HardwiredControl::getMicroPC() const { return 0; }

}  // namespace risc201
