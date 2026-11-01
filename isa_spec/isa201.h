#pragma once

// RISC201 ISA definitions, frozen 25 Sept.
// Every module includes this file.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace risc201 {

// 16 general purpose registers, 4096 words of memory.
inline constexpr std::size_t kRegisterCount = 16;
inline constexpr std::size_t kMemoryWords = 4096;

// Memory map.
// code 0x000-0x7FF, data 0x800-0xBFF, stack 0xC00-0xFFF.
// The stack starts empty at 0x1000 and grows downwards.
inline constexpr uint32_t kCodeBase = 0x000;
inline constexpr uint32_t kCodeLimit = 0x7FF;
inline constexpr uint32_t kDataBase = 0x800;
inline constexpr uint32_t kDataLimit = 0xBFF;
inline constexpr uint32_t kStackBase = 0xC00;
inline constexpr uint32_t kStackLimit = 0xFFF;
inline constexpr uint32_t kInitialSp = 0x1000;
inline constexpr uint32_t kLastAddress = 0xFFF;

// Encoding.
// opcode is bits 31-26.
// R-type: op[6] rd[4] rs[4] rt[4] unused[14]
// I-type: op[6] rd[4] rs[4] imm[18] signed
// J-type: op[6] offset[26] signed, relative to PC+1
inline constexpr unsigned kOpcodeShift = 26;
inline constexpr uint32_t kOpcodeMask = 0x3Fu;
inline constexpr unsigned kRdShift = 22;
inline constexpr unsigned kRsShift = 18;
inline constexpr unsigned kRtShift = 14;
inline constexpr uint32_t kRegisterMask = 0xFu;
inline constexpr unsigned kImmediateBits = 18;
inline constexpr unsigned kOffsetBits = 26;
inline constexpr uint32_t kImmediateMask = 0x3FFFFu;
inline constexpr uint32_t kOffsetMask = 0x3FFFFFFu;

// The original 27 instructions plus four floating-point extensions.
enum class Opcode : uint8_t {
  NOP = 0x00,
  HALT = 0x01,
  ADD = 0x02,
  SUB = 0x03,
  MUL = 0x04,
  DIV = 0x05,
  AND = 0x06,
  OR = 0x07,
  XOR = 0x08,
  NOT = 0x09,
  SHL = 0x0A,
  SHR = 0x0B,
  MOV = 0x0C,
  CMP = 0x0D,
  FADD = 0x17,
  FSUB = 0x18,
  FMUL = 0x19,
  FDIV = 0x1A,
  ADDI = 0x10,
  LDI = 0x11,
  LW = 0x12,
  SW = 0x13,
  PUSH = 0x14,
  POP = 0x15,
  RET = 0x16,
  JMP = 0x20,
  BEQ = 0x21,
  BNE = 0x22,
  BLT = 0x23,
  BGE = 0x24,
  CALL = 0x25,
};

// Register, Immediate, Jump.
enum class InstructionFormat : uint8_t {
  None,
  R,
  I,
  J,
};

struct InstructionInfo {
  Opcode opcode;
  const char *mnemonic;
  InstructionFormat format;
};

inline constexpr InstructionInfo kInstructionTable[] = {
    {Opcode::NOP, "NOP", InstructionFormat::None},
    {Opcode::HALT, "HALT", InstructionFormat::None},
    {Opcode::ADD, "ADD", InstructionFormat::R},
    {Opcode::SUB, "SUB", InstructionFormat::R},
    {Opcode::MUL, "MUL", InstructionFormat::R},
    {Opcode::DIV, "DIV", InstructionFormat::R},
    {Opcode::AND, "AND", InstructionFormat::R},
    {Opcode::OR, "OR", InstructionFormat::R},
    {Opcode::XOR, "XOR", InstructionFormat::R},
    {Opcode::NOT, "NOT", InstructionFormat::R},
    {Opcode::SHL, "SHL", InstructionFormat::R},
    {Opcode::SHR, "SHR", InstructionFormat::R},
    {Opcode::MOV, "MOV", InstructionFormat::R},
    {Opcode::CMP, "CMP", InstructionFormat::R},
    {Opcode::FADD, "FADD", InstructionFormat::R},
    {Opcode::FSUB, "FSUB", InstructionFormat::R},
    {Opcode::FMUL, "FMUL", InstructionFormat::R},
    {Opcode::FDIV, "FDIV", InstructionFormat::R},
    {Opcode::ADDI, "ADDI", InstructionFormat::I},
    {Opcode::LDI, "LDI", InstructionFormat::I},
    {Opcode::LW, "LW", InstructionFormat::I},
    {Opcode::SW, "SW", InstructionFormat::I},
    {Opcode::PUSH, "PUSH", InstructionFormat::I},
    {Opcode::POP, "POP", InstructionFormat::I},
    {Opcode::RET, "RET", InstructionFormat::I},
    {Opcode::JMP, "JMP", InstructionFormat::J},
    {Opcode::BEQ, "BEQ", InstructionFormat::J},
    {Opcode::BNE, "BNE", InstructionFormat::J},
    {Opcode::BLT, "BLT", InstructionFormat::J},
    {Opcode::BGE, "BGE", InstructionFormat::J},
    {Opcode::CALL, "CALL", InstructionFormat::J},
};

inline constexpr std::size_t kInstructionCount =
    sizeof(kInstructionTable) / sizeof(kInstructionTable[0]);

// Returns nullptr if the opcode is not in the table.
inline constexpr const InstructionInfo *lookupInstruction(Opcode opcode) {
  for (std::size_t i = 0; i < kInstructionCount; ++i) {
    if (kInstructionTable[i].opcode == opcode) {
      return &kInstructionTable[i];
    }
  }
  return nullptr;
}

// Used by the control unit to raise ILLEGAL_INSTRUCTION.
inline constexpr bool isDefinedOpcode(Opcode opcode) {
  return lookupInstruction(opcode) != nullptr;
}

inline constexpr const char *mnemonicOf(Opcode opcode) {
  const InstructionInfo *info = lookupInstruction(opcode);
  return info != nullptr ? info->mnemonic : "???";
}

inline constexpr InstructionFormat formatOf(Opcode opcode) {
  const InstructionInfo *info = lookupInstruction(opcode);
  return info != nullptr ? info->format : InstructionFormat::None;
}

// Zero, Negative, Carry, oVerflow.
// ALU status flags
struct Flags {
  bool z = false;
  bool n = false;
  bool c = false;
  bool v = false;
};

inline constexpr bool operator==(const Flags &lhs, const Flags &rhs) {
  return lhs.z == rhs.z && lhs.n == rhs.n && lhs.c == rhs.c && lhs.v == rhs.v;
}

inline constexpr bool operator!=(const Flags &lhs, const Flags &rhs) {
  return !(lhs == rhs);
}

// List of things that can crash the program.
enum class ExceptionCode : uint8_t {
  NONE = 0,
  STACK_OVERFLOW = 1,
  STACK_UNDERFLOW = 2,
  INVALID_MEM_ACCESS = 3,
  DIV_BY_ZERO = 4,
  ILLEGAL_INSTRUCTION = 5,
  PC_OUT_OF_RANGE = 6,
};

// Convert exception code to string.
inline std::string toString(ExceptionCode code) {
  switch (code) {
  case ExceptionCode::NONE:
    return "NONE";
  case ExceptionCode::STACK_OVERFLOW:
    return "STACK_OVERFLOW";
  case ExceptionCode::STACK_UNDERFLOW:
    return "STACK_UNDERFLOW";
  case ExceptionCode::INVALID_MEM_ACCESS:
    return "INVALID_MEM_ACCESS";
  case ExceptionCode::DIV_BY_ZERO:
    return "DIV_BY_ZERO";
  case ExceptionCode::ILLEGAL_INSTRUCTION:
    return "ILLEGAL_INSTRUCTION";
  case ExceptionCode::PC_OUT_OF_RANGE:
    return "PC_OUT_OF_RANGE";
  }
  return "UNKNOWN";
}

// Operations the ALU has to implement.
enum class AluOp : uint8_t {
  ADD,
  SUB,
  MUL,
  DIV,
  AND,
  OR,
  XOR,
  NOT,
  SHL,
  SHR,
  MOV,
  CMP,
  FADD, FSUB, FMUL, FDIV,
};

struct AluResult {
  uint32_t value = 0;
  Flags flags{};
  ExceptionCode exception = ExceptionCode::NONE;
};

enum class AluSrc : uint8_t {
  Register,
  Immediate,
};

enum class ResultSrc : uint8_t {
  Alu,
  Memory,
};

// What the control unit's decode() returns for one instruction.
struct ControlSignals {
  Opcode opcode = Opcode::NOP;
  AluOp aluOp = AluOp::ADD;
  AluSrc aluSrc = AluSrc::Register;
  ResultSrc resultSrc = ResultSrc::Alu;
  bool regWrite = false;
  bool memRead = false;
  bool memWrite = false;
  bool branch = false;
  bool jump = false;
  bool push = false;
  bool pop = false;
  bool ret = false;
  bool halt = false;
  bool writeFlags = false;
  bool illegal = false;
};

// Sign extends the low bits of value.
inline constexpr int32_t signExtend(uint32_t value, unsigned bits) {
  const uint32_t signBit = 1u << (bits - 1);
  return static_cast<int32_t>((value ^ signBit) - signBit);
}

inline constexpr uint32_t encodeOpcode(Opcode opcode) {
  return static_cast<uint32_t>(opcode) << kOpcodeShift;
}

inline constexpr uint32_t encodeR(Opcode opcode, uint8_t rd, uint8_t rs,
                                  uint8_t rt) {
  return encodeOpcode(opcode) |
         ((static_cast<uint32_t>(rd) & kRegisterMask) << kRdShift) |
         ((static_cast<uint32_t>(rs) & kRegisterMask) << kRsShift) |
         ((static_cast<uint32_t>(rt) & kRegisterMask) << kRtShift);
}

inline constexpr uint32_t encodeI(Opcode opcode, uint8_t rd, uint8_t rs,
                                  int32_t imm) {
  return encodeOpcode(opcode) |
         ((static_cast<uint32_t>(rd) & kRegisterMask) << kRdShift) |
         ((static_cast<uint32_t>(rs) & kRegisterMask) << kRsShift) |
         (static_cast<uint32_t>(imm) & kImmediateMask);
}

inline constexpr uint32_t encodeJ(Opcode opcode, int32_t offset) {
  return encodeOpcode(opcode) | (static_cast<uint32_t>(offset) & kOffsetMask);
}

inline constexpr Opcode decodeOpcode(uint32_t word) {
  return static_cast<Opcode>((word >> kOpcodeShift) & kOpcodeMask);
}

inline constexpr uint8_t decodeRd(uint32_t word) {
  return static_cast<uint8_t>((word >> kRdShift) & kRegisterMask);
}

inline constexpr uint8_t decodeRs(uint32_t word) {
  return static_cast<uint8_t>((word >> kRsShift) & kRegisterMask);
}

inline constexpr uint8_t decodeRt(uint32_t word) {
  return static_cast<uint8_t>((word >> kRtShift) & kRegisterMask);
}

inline constexpr int32_t decodeImmediate(uint32_t word) {
  return signExtend(word & kImmediateMask, kImmediateBits);
}

inline constexpr int32_t decodeOffset(uint32_t word) {
  return signExtend(word & kOffsetMask, kOffsetBits);
}

} // namespace risc201
