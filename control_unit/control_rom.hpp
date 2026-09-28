#pragma once

#include <cstdint>
#include <array>
#include "isa_spec/isa201.h"
#include "control_unit/microcode.hpp"

namespace risc201 {

namespace {

// helper to easily pack the ALU operation into our 32-bit ROM word
constexpr uint32_t packAlu(AluOp op) {
  return (static_cast<uint32_t>(op) & MicrocodeBits::ALU_OP_MASK) << MicrocodeBits::ALU_OP_SHIFT;
}

// function to burn the microcode into our ROM array at compile time!
constexpr std::array<uint32_t, 64> buildControlRom() {
  std::array<uint32_t, 64> rom{};
  
  // start by making everything illegal so we crash on bad opcodes
  for (int i = 0; i < 64; ++i) {
    rom[i] = MicrocodeBits::ILLEGAL;
  }

  using namespace MicrocodeBits;

  // NOP (all zeroes)
  rom[static_cast<size_t>(Opcode::NOP)] = 0;
  
  // HALT
  rom[static_cast<size_t>(Opcode::HALT)] = HALT;
  
  // ALU R-Type operations
  rom[static_cast<size_t>(Opcode::ADD)] = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::ADD);
  rom[static_cast<size_t>(Opcode::SUB)] = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::SUB);
  rom[static_cast<size_t>(Opcode::MUL)] = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::MUL);
  rom[static_cast<size_t>(Opcode::DIV)] = REG_WRITE | packAlu(AluOp::DIV); 
  rom[static_cast<size_t>(Opcode::AND)] = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::AND);
  rom[static_cast<size_t>(Opcode::OR)]  = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::OR);
  rom[static_cast<size_t>(Opcode::XOR)] = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::XOR);
  rom[static_cast<size_t>(Opcode::NOT)] = REG_WRITE | WRITE_FLAGS | packAlu(AluOp::NOT);
  rom[static_cast<size_t>(Opcode::SHL)] = REG_WRITE | packAlu(AluOp::SHL); 
  rom[static_cast<size_t>(Opcode::SHR)] = REG_WRITE | packAlu(AluOp::SHR);
  rom[static_cast<size_t>(Opcode::MOV)] = REG_WRITE | packAlu(AluOp::MOV);
  
  // CMP only sets flags, doesnt write to register!
  rom[static_cast<size_t>(Opcode::CMP)] = WRITE_FLAGS | packAlu(AluOp::CMP); 

  // ALU I-Type operations (needs immediate)
  rom[static_cast<size_t>(Opcode::ADDI)] = REG_WRITE | WRITE_FLAGS | ALU_SRC_IMM | packAlu(AluOp::ADD);
  rom[static_cast<size_t>(Opcode::LDI)]  = REG_WRITE | ALU_SRC_IMM | packAlu(AluOp::MOV);

  // Memory Load/Store
  rom[static_cast<size_t>(Opcode::LW)] = REG_WRITE | MEM_READ | ALU_SRC_IMM | RES_SRC_MEM | packAlu(AluOp::ADD);
  rom[static_cast<size_t>(Opcode::SW)] = MEM_WRITE | ALU_SRC_IMM | packAlu(AluOp::ADD);

  // Stack operations
  rom[static_cast<size_t>(Opcode::PUSH)] = PUSH;
  rom[static_cast<size_t>(Opcode::POP)]  = POP | REG_WRITE;
  rom[static_cast<size_t>(Opcode::RET)]  = RET;

  // Control Flow (Branch and Jump)
  rom[static_cast<size_t>(Opcode::JMP)]  = JUMP;
  rom[static_cast<size_t>(Opcode::BEQ)]  = BRANCH; 
  rom[static_cast<size_t>(Opcode::BNE)]  = BRANCH;
  rom[static_cast<size_t>(Opcode::BLT)]  = BRANCH;
  rom[static_cast<size_t>(Opcode::BGE)]  = BRANCH;
  
  // CALL is basically a JUMP and a PUSH combined!
  rom[static_cast<size_t>(Opcode::CALL)] = JUMP | PUSH; 

  return rom;
}

// this is the actual physical ROM chip simulated in memory
inline constexpr std::array<uint32_t, 64> CONTROL_ROM = buildControlRom();

} // namespace
} // namespace risc201