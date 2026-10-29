#pragma once

// Vertical microcode for the control unit.
//
// The horizontal control ROM uses one wide word per instruction: every control
// wire gets its own bit, so there is nothing to decode but the words are wide.
// Vertical microcode goes the other way: each control word is a narrow encoded
// micro operation, and a decoder plus a sequencer turn a short program of those
// operations into the signals the pipeline needs.

#include <array>
#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

// One encoded micro operation. A microprogram for a single machine instruction
// is a short sequence of these ending in END.
enum class MicroOp : uint8_t {
  END = 0,   // stop the sequencer, the instruction is done
  ALU_REG,   // run the ALU with the second operand from a register
  ALU_IMM,   // run the ALU with the second operand from the immediate
  REG_WRITE, // write the ALU or memory result back to rd
  FLAGS,     // commit the ALU flags
  LOAD,      // memory read into rd
  STORE,     // memory write from rd
  PUSH,      // decrement SP and store
  POP,       // load from SP and increment
  RET,       // PC = mem[SP], then SP increments
  JUMP,      // PC = PC + 1 + offset
  BRANCH,    // conditional PC = PC + 1 + offset
  HALT,      // stop the machine
};

// A vertical control word is 16 bits: four bits pick the micro operation and
// four bits carry the ALU op. The top byte is left free for the sequencer.
inline constexpr uint16_t encodeMicroOp(MicroOp op, AluOp alu = AluOp::ADD) {
  return static_cast<uint16_t>((static_cast<uint16_t>(op) & 0x0Fu) |
                               ((static_cast<uint16_t>(alu) & 0x0Fu) << 4));
}

inline constexpr MicroOp microOpOf(uint16_t word) {
  return static_cast<MicroOp>(word & 0x0Fu);
}

inline constexpr AluOp microAluOf(uint16_t word) {
  return static_cast<AluOp>((word >> 4) & 0x0Fu);
}

// Room for every instruction's microprogram, including the floating-point extension.
inline constexpr std::size_t kMicroProgramSlots = 128;

// Sentinel in the start table for opcodes that have no microprogram, which the
// sequencer reports as an illegal instruction.
inline constexpr uint16_t kNoMicroProgram = 0xFFFFu;

struct MicroProgram {
  // Flat store of micro words, walked by index.
  std::array<uint16_t, kMicroProgramSlots> words{};
  // start[opcode] is the index of the first micro word, or kNoMicroProgram.
  std::array<uint16_t, 64> start{};
};

namespace detail {

struct MicroBuilder {
  MicroProgram program{};
  std::size_t at = 0;

  constexpr void begin(Opcode opcode) {
    program.start[static_cast<std::size_t>(opcode)] = static_cast<uint16_t>(at);
  }

  constexpr void word(MicroOp op, AluOp alu = AluOp::ADD) {
    program.words[at++] = encodeMicroOp(op, alu);
  }
};

} // namespace detail

// Burn the microprogram in at compile time. Each machine instruction decodes to
// the same set of control signals as its horizontal ROM word.
constexpr MicroProgram buildMicroProgram() {
  detail::MicroBuilder b{};
  for (std::size_t i = 0; i < b.program.start.size(); ++i) {
    b.program.start[i] = kNoMicroProgram;
  }

  using MO = MicroOp;

  b.begin(Opcode::NOP);
  b.word(MO::END);

  b.begin(Opcode::HALT);
  b.word(MO::HALT);
  b.word(MO::END);

  b.begin(Opcode::ADD);
  b.word(MO::ALU_REG, AluOp::ADD);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::SUB);
  b.word(MO::ALU_REG, AluOp::SUB);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::MUL);
  b.word(MO::ALU_REG, AluOp::MUL);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::DIV);
  b.word(MO::ALU_REG, AluOp::DIV);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::AND);
  b.word(MO::ALU_REG, AluOp::AND);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::OR);
  b.word(MO::ALU_REG, AluOp::OR);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::XOR);
  b.word(MO::ALU_REG, AluOp::XOR);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::NOT);
  b.word(MO::ALU_REG, AluOp::NOT);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::SHL);
  b.word(MO::ALU_REG, AluOp::SHL);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::SHR);
  b.word(MO::ALU_REG, AluOp::SHR);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::MOV);
  b.word(MO::ALU_REG, AluOp::MOV);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::CMP);
  b.word(MO::ALU_REG, AluOp::CMP);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::FADD);
  b.word(MO::ALU_REG, AluOp::FADD);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::FSUB);
  b.word(MO::ALU_REG, AluOp::FSUB);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::FMUL);
  b.word(MO::ALU_REG, AluOp::FMUL);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::FDIV);
  b.word(MO::ALU_REG, AluOp::FDIV);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::ADDI);
  b.word(MO::ALU_IMM, AluOp::ADD);
  b.word(MO::REG_WRITE);
  b.word(MO::FLAGS);
  b.word(MO::END);

  b.begin(Opcode::LDI);
  b.word(MO::ALU_IMM, AluOp::ADD);
  b.word(MO::REG_WRITE);
  b.word(MO::END);

  b.begin(Opcode::LW);
  b.word(MO::ALU_IMM, AluOp::ADD);
  b.word(MO::LOAD);
  b.word(MO::END);

  b.begin(Opcode::SW);
  b.word(MO::ALU_IMM, AluOp::ADD);
  b.word(MO::STORE);
  b.word(MO::END);

  b.begin(Opcode::PUSH);
  b.word(MO::PUSH);
  b.word(MO::END);

  b.begin(Opcode::POP);
  b.word(MO::POP);
  b.word(MO::END);

  b.begin(Opcode::RET);
  b.word(MO::RET);
  b.word(MO::END);

  b.begin(Opcode::JMP);
  b.word(MO::JUMP);
  b.word(MO::END);

  b.begin(Opcode::BEQ);
  b.word(MO::BRANCH);
  b.word(MO::END);

  b.begin(Opcode::BNE);
  b.word(MO::BRANCH);
  b.word(MO::END);

  b.begin(Opcode::BLT);
  b.word(MO::BRANCH);
  b.word(MO::END);

  b.begin(Opcode::BGE);
  b.word(MO::BRANCH);
  b.word(MO::END);

  b.begin(Opcode::CALL);
  b.word(MO::JUMP);
  b.word(MO::PUSH);
  b.word(MO::END);

  return b.program;
}

inline constexpr MicroProgram kMicroProgram = buildMicroProgram();

// How many micro words an opcode's program holds, END included. Zero when the
// opcode has no microprogram.
inline constexpr std::size_t microProgramLength(Opcode opcode) {
  const uint16_t start = kMicroProgram.start[static_cast<std::size_t>(opcode)];
  if (start == kNoMicroProgram) {
    return 0;
  }
  std::size_t count = 0;
  while (kMicroProgram.words[start + count] !=
         encodeMicroOp(MicroOp::END)) {
    ++count;
  }
  return count + 1;
}

} // namespace risc201
