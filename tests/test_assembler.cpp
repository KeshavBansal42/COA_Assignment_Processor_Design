#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "assembler/assembler.hpp"
#include "isa_spec/isa201.h"

namespace {

void writeTempFile(const std::string& path, const std::string& content) {
  std::ofstream f(path);
  assert(f.is_open());
  f << content;
}

void testAll27Instructions() {
  const std::string asmPath = "test_all_instructions.s";
  const std::string src =
      "; Test all 27 instructions\n"
      "NOP\n"
      "HALT\n"
      "ADD R1, R2, R3\n"
      "SUB R4, R5, R6\n"
      "MUL R7, R8, R9\n"
      "DIV R10, R11, R12\n"
      "AND R13, R14, R15\n"
      "OR R1, R2, R3\n"
      "XOR R4, R5, R6\n"
      "NOT R7, R8\n"
      "SHL R9, R10, R11\n"
      "SHR R12, R13, R14\n"
      "MOV R15, R1\n"
      "CMP R2, R3\n"
      "ADDI R4, R5, -42\n"
      "LDI R6, 12345\n"
      "LW R7, 8(R8)\n"
      "SW R9, -16(R10)\n"
      "PUSH R11\n"
      "POP R12\n"
      "RET\n"
      "target:\n"
      "JMP target\n"
      "BEQ target\n"
      "BNE target\n"
      "BLT target\n"
      "BGE target\n"
      "CALL target\n";

  writeTempFile(asmPath, src);

  std::vector<uint32_t> program;
  risc201::assemble(asmPath, program);
  assert(program.size() == 27);

  assert(program[0] == risc201::encodeOpcode(risc201::Opcode::NOP));
  assert(program[1] == risc201::encodeOpcode(risc201::Opcode::HALT));
  assert(program[2] == risc201::encodeR(risc201::Opcode::ADD, 1, 2, 3));
  assert(program[3] == risc201::encodeR(risc201::Opcode::SUB, 4, 5, 6));
  assert(program[4] == risc201::encodeR(risc201::Opcode::MUL, 7, 8, 9));
  assert(program[5] == risc201::encodeR(risc201::Opcode::DIV, 10, 11, 12));
  assert(program[6] == risc201::encodeR(risc201::Opcode::AND, 13, 14, 15));
  assert(program[7] == risc201::encodeR(risc201::Opcode::OR, 1, 2, 3));
  assert(program[8] == risc201::encodeR(risc201::Opcode::XOR, 4, 5, 6));
  assert(program[9] == risc201::encodeR(risc201::Opcode::NOT, 7, 8, 0));
  assert(program[10] == risc201::encodeR(risc201::Opcode::SHL, 9, 10, 11));
  assert(program[11] == risc201::encodeR(risc201::Opcode::SHR, 12, 13, 14));
  assert(program[12] == risc201::encodeR(risc201::Opcode::MOV, 15, 1, 0));
  assert(program[13] == risc201::encodeR(risc201::Opcode::CMP, 0, 2, 3));
  assert(program[14] == risc201::encodeI(risc201::Opcode::ADDI, 4, 5, -42));
  assert(program[15] == risc201::encodeI(risc201::Opcode::LDI, 6, 0, 12345));
  assert(program[16] == risc201::encodeI(risc201::Opcode::LW, 7, 8, 8));
  assert(program[17] == risc201::encodeI(risc201::Opcode::SW, 9, 10, -16));
  assert(program[18] == risc201::encodeI(risc201::Opcode::PUSH, 11, 0, 0));
  assert(program[19] == risc201::encodeI(risc201::Opcode::POP, 12, 0, 0));
  assert(program[20] == risc201::encodeI(risc201::Opcode::RET, 0, 0, 0));
  assert(program[21] == risc201::encodeJ(risc201::Opcode::JMP, -1));
  assert(program[22] == risc201::encodeJ(risc201::Opcode::BEQ, -2));
  assert(program[23] == risc201::encodeJ(risc201::Opcode::BNE, -3));
  assert(program[24] == risc201::encodeJ(risc201::Opcode::BLT, -4));
  assert(program[25] == risc201::encodeJ(risc201::Opcode::BGE, -5));
  assert(program[26] == risc201::encodeJ(risc201::Opcode::CALL, -6));

  // Verify pure hex output file was created beside the .s file
  std::ifstream hexFile("test_all_instructions.hex");
  assert(hexFile.is_open());
  std::string hexLine;
  size_t count = 0;
  while (std::getline(hexFile, hexLine)) {
    assert(hexLine.size() == 8);
    count++;
  }
  assert(count == 27);
}

void testStrictUppercaseCasing() {
  // Lowercase mnemonic must fail
  {
    const std::string path = "test_case_mnemonic.s";
    writeTempFile(path, "add R1, R2, R3\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      const std::string msg = e.what();
      assert(msg.find("Line 1") != std::string::npos);
      assert(msg.find("uppercase") != std::string::npos);
    }
    assert(threw);
  }

  // Lowercase register must fail
  {
    const std::string path = "test_case_reg.s";
    writeTempFile(path, "ADD r1, R2, R3\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      const std::string msg = e.what();
      assert(msg.find("Line 1") != std::string::npos);
      assert(msg.find("invalid register") != std::string::npos);
    }
    assert(threw);
  }
}

void testLabelsStandalone() {
  const std::string path = "test_labels.s";
  const std::string src =
      "; Loop program using standalone labels only\n"
      "LDI R1, 10\n"
      "LDI R2, 0\n"
      "loop:\n"
      "  ADDI R1, R1, -1\n"
      "  CMP R1, R2\n"
      "  BNE loop\n"
      "done:\n"
      "  HALT\n";

  writeTempFile(path, src);

  std::vector<uint32_t> program;
  risc201::assemble(path, program);
  assert(program.size() == 6);

  // Address 0: LDI R1, 10
  // Address 1: LDI R2, 0
  // Address 2: ADDI R1, R1, -1  (loop is at address 2)
  // Address 3: CMP R1, R2
  // Address 4: BNE loop -> offset = 2 - (4 + 1) = -3
  // Address 5: HALT (done is at address 5)
  assert(program[4] == risc201::encodeJ(risc201::Opcode::BNE, -3));
  assert(program[5] == risc201::encodeOpcode(risc201::Opcode::HALT));
}

void testForwardAndBackwardCalls() {
  const std::string path = "test_calls.s";
  const std::string src =
      "CALL helper\n"
      "HALT\n"
      "helper:\n"
      "  RET\n";

  writeTempFile(path, src);

  std::vector<uint32_t> program;
  risc201::assemble(path, program);
  assert(program.size() == 3);

  // Address 0: CALL helper (helper is address 2, offset = 2 - (0 + 1) = +1)
  // Address 1: HALT
  // Address 2: RET
  assert(program[0] == risc201::encodeJ(risc201::Opcode::CALL, 1));
  assert(program[1] == risc201::encodeOpcode(risc201::Opcode::HALT));
  assert(program[2] == risc201::encodeI(risc201::Opcode::RET, 0, 0, 0));
}

void testErrorHandling() {
  // Undefined symbol
  {
    const std::string path = "test_undef_sym.s";
    writeTempFile(path, "JMP nonexistent\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      assert(std::string(e.what()).find("undefined label") != std::string::npos);
    }
    assert(threw);
  }

  // Duplicate label pointing to the second declaration line (line 3)
  {
    const std::string path = "test_dup_label.s";
    writeTempFile(path, "start:\n  NOP\nstart:\n  NOP\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      const std::string msg = e.what();
      assert(msg.find("Line 3") != std::string::npos);
      assert(msg.find("duplicate label") != std::string::npos);
    }
    assert(threw);
  }

  // Inline label must be rejected
  {
    const std::string path = "test_inline_label.s";
    writeTempFile(path, "done: HALT\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      assert(std::string(e.what()).find("inline labels not allowed") != std::string::npos);
    }
    assert(threw);
  }

  // Manual numeric PC offset must be rejected (labels only)
  {
    const std::string path = "test_numeric_jump.s";
    writeTempFile(path, "JMP -5\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      assert(std::string(e.what()).find("manual PC offset not allowed") != std::string::npos);
    }
    assert(threw);
  }

  // Bare (RS) without immediate must be rejected (must be 0(RS))
  {
    const std::string path = "test_bare_mem.s";
    writeTempFile(path, "LW R1, (R2)\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      assert(std::string(e.what()).find("invalid memory operand syntax") != std::string::npos);
    }
    assert(threw);
  }

  // Register out of range
  {
    const std::string path = "test_reg_oob.s";
    writeTempFile(path, "ADD R16, R1, R2\n");
    bool threw = false;
    try {
      risc201::assemble(path);
    } catch (const std::runtime_error& e) {
      threw = true;
      assert(std::string(e.what()).find("register index out of range") != std::string::npos);
    }
    assert(threw);
  }

  // Missing file
  {
    bool threw = false;
    try {
      risc201::assemble("non_existent_file_12345.s");
    } catch (const std::runtime_error&) {
      threw = true;
    }
    assert(threw);
  }
}

void cleanTempFiles() {
  std::remove("test_all_instructions.s");
  std::remove("test_all_instructions.hex");
  std::remove("test_case_mnemonic.s");
  std::remove("test_case_reg.s");
  std::remove("test_labels.s");
  std::remove("test_labels.hex");
  std::remove("test_calls.s");
  std::remove("test_calls.hex");
  std::remove("test_undef_sym.s");
  std::remove("test_dup_label.s");
  std::remove("test_inline_label.s");
  std::remove("test_numeric_jump.s");
  std::remove("test_bare_mem.s");
  std::remove("test_reg_oob.s");
}

}  // namespace

int main() {
  cleanTempFiles();
  testAll27Instructions();
  testStrictUppercaseCasing();
  testLabelsStandalone();
  testForwardAndBackwardCalls();
  testErrorHandling();
  cleanTempFiles();
  std::cout << "All assembler unit tests passed!\n";
  return 0;
}
