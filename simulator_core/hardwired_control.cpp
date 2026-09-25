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

    case Opcode::MUL:
      signals.aluOp = AluOp::MUL;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::DIV:
      signals.aluOp = AluOp::DIV;
      signals.regWrite = true;
      break;

    case Opcode::AND:
      signals.aluOp = AluOp::AND;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::OR:
      signals.aluOp = AluOp::OR;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::XOR:
      signals.aluOp = AluOp::XOR;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::NOT:
      signals.aluOp = AluOp::NOT;
      signals.regWrite = true;
      signals.writeFlags = true;
      break;

    case Opcode::SHL:
      signals.aluOp = AluOp::SHL;
      signals.regWrite = true;
      break;

    case Opcode::SHR:
      signals.aluOp = AluOp::SHR;
      signals.regWrite = true;
      break;

    case Opcode::MOV:
      signals.aluOp = AluOp::MOV;
      signals.regWrite = true;
      break;

    case Opcode::CMP:
      signals.aluOp = AluOp::CMP;
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

    case Opcode::PUSH:
      signals.push = true;
      signals.memWrite = true;
      break;

    case Opcode::POP:
      signals.pop = true;
      signals.memRead = true;
      signals.resultSrc = ResultSrc::Memory;
      signals.regWrite = true;
      break;

    case Opcode::RET:
      signals.ret = true;
      signals.memRead = true;
      break;

    case Opcode::JMP:
      signals.jump = true;
      break;

    case Opcode::BEQ:
    case Opcode::BNE:
    case Opcode::BLT:
    case Opcode::BGE:
      signals.branch = true;
      break;

    case Opcode::CALL:
      signals.jump = true;
      signals.push = true;
      signals.memWrite = true;
      break;
  }

  return signals;
}

uint32_t HardwiredControl::getMicroPC() const { return 0; }

}  // namespace risc201
