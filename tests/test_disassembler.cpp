#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "assembler/assembler.hpp"
#include "disassembler/disassembler.hpp"
#include "isa_spec/isa201.h"

namespace {

void writeHexFile(const std::string& path, const std::vector<std::string>& words) {
  std::ofstream f(path);
  assert(f.is_open());
  for (const auto& w : words) {
    f << w << "\n";
  }
}

std::vector<std::string> readLines(const std::string& path) {
  std::ifstream f(path);
  assert(f.is_open());
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(f, line)) {
    lines.push_back(line);
  }
  return lines;
}

void testAll27InstructionsRoundTrip() {
  const std::string sPath = "test_all27.s";
  const std::string hexPath = "test_all27.hex";
  const std::string src =
      "  NOP\n"
      "  HALT\n"
      "  ADD R1, R2, R3\n"
      "  SUB R4, R5, R6\n"
      "  MUL R7, R8, R9\n"
      "  DIV R10, R11, R12\n"
      "  AND R13, R14, R15\n"
      "  OR R1, R2, R3\n"
      "  XOR R4, R5, R6\n"
      "  NOT R7, R8\n"
      "  SHL R9, R10, R11\n"
      "  SHR R12, R13, R14\n"
      "  MOV R15, R1\n"
      "  CMP R2, R3\n"
      "  ADDI R4, R5, -42\n"
      "  LDI R6, 12345\n"
      "  LW R7, 8(R8)\n"
      "  SW R9, -16(R10)\n"
      "  PUSH R11\n"
      "POP R12\n"
      "RET\n"
      "target:\n"
      "  JMP target\n"
      "  BEQ target\n"
      "  BNE target\n"
      "  BLT target\n"
      "  BGE target\n"
      "  CALL target\n";

  {
    std::ofstream f(sPath);
    assert(f.is_open());
    f << src;
  }

  // 1. Assemble to .hex
  risc201::assemble(sPath);
  const auto originalHex = readLines(hexPath);
  assert(originalHex.size() == 27);

  // 2. Remove .s, then disassemble .hex -> .s
  std::remove(sPath.c_str());
  risc201::disassemble(hexPath);

  // 3. Verify .s exists and contains synthetic label L_21
  const auto disLines = readLines(sPath);
  bool foundLabel = false;
  for (const auto& l : disLines) {
    if (l == "L_21:") {
      foundLabel = true;
      break;
    }
  }
  assert(foundLabel);

  // 4. Re-assemble disassembled .s -> .hex
  risc201::assemble(sPath);
  const auto reassembledHex = readLines(hexPath);

  // 5. Binary round-trip check: must be byte-for-byte identical
  assert(originalHex == reassembledHex);

  std::remove(sPath.c_str());
  std::remove(hexPath.c_str());
}

void testIllegalOpcode() {
  const std::string hexPath = "test_illegal.hex";
  writeHexFile(hexPath, {"FC000000"});  // Opcode 0x3F (undefined)

  bool caught = false;
  try {
    risc201::disassemble(hexPath);
  } catch (const std::runtime_error& e) {
    caught = true;
    const std::string msg = e.what();
    assert(msg.find("illegal opcode: 0x3F") != std::string::npos);
  }
  assert(caught);

  std::remove(hexPath.c_str());
}

void testOutOfBoundsBranch() {
  const std::string hexPath = "test_oob.hex";
  const std::string sPath = "test_oob.s";
  // JMP with offset +10 at PC 0 (targets PC 11, but file has only 1 instruction)
  // JMP opcode 0x20 << 26 = 0x80000000, offset 10 = 0x0000000A -> 0x8000000A
  writeHexFile(hexPath, {"8000000A"});

  risc201::disassemble(hexPath);

  const auto lines = readLines(sPath);
  assert(lines.size() == 1);
  assert(lines[0] == "  JMP L_TARGET  ; PC -> PC + 10, Branch Target out of bounds");

  std::remove(hexPath.c_str());
  std::remove(sPath.c_str());
}

void testForwardAndBackwardBranches() {
  const std::string sPath = "test_branches.s";
  const std::string hexPath = "test_branches.hex";
  const std::string src =
      "  LDI R1, 5\n"
      "loop:\n"
      "  ADDI R1, R1, -1\n"
      "  CMP R1, R0\n"
      "  BNE loop\n"
      "  BEQ end\n"
      "  NOP\n"
      "end:\n"
      "  HALT\n";

  {
    std::ofstream f(sPath);
    f << src;
  }

  risc201::assemble(sPath);
  const auto originalHex = readLines(hexPath);

  std::remove(sPath.c_str());
  risc201::disassemble(hexPath);

  const auto disLines = readLines(sPath);
  // Verify labels L_1 and L_6 are emitted
  bool hasL1 = false;
  bool hasL6 = false;
  for (const auto& l : disLines) {
    if (l == "L_1:") hasL1 = true;
    if (l == "L_6:") hasL6 = true;
  }
  assert(hasL1 && hasL6);

  risc201::assemble(sPath);
  const auto reassembledHex = readLines(hexPath);
  assert(originalHex == reassembledHex);

  std::remove(sPath.c_str());
  std::remove(hexPath.c_str());
}

void testMissingFile() {
  bool caught = false;
  try {
    risc201::disassemble("non_existent_file_xyz.hex");
  } catch (const std::runtime_error&) {
    caught = true;
  }
  assert(caught);
}

void testExceedsCodeLimit() {
  const std::string hexPath = "test_overflow.hex";
  std::vector<std::string> words(risc201::kCodeLimit + 2, "00000000");
  writeHexFile(hexPath, words);

  bool caught = false;
  try {
    risc201::disassemble(hexPath);
  } catch (const std::runtime_error& e) {
    caught = true;
    const std::string msg = e.what();
    assert(msg.find("exceeds code region limit") != std::string::npos);
  }
  assert(caught);

  std::remove(hexPath.c_str());
}

}  // namespace

int main() {
  std::cout << "Running RISC201 Disassembler Test Suite...\n";
  testAll27InstructionsRoundTrip();
  std::cout << "  [PASS] All 27 instructions round-trip\n";
  testIllegalOpcode();
  std::cout << "  [PASS] Illegal opcode exception\n";
  testOutOfBoundsBranch();
  std::cout << "  [PASS] Out-of-bounds branch target comment\n";
  testForwardAndBackwardBranches();
  std::cout << "  [PASS] Forward and backward branches with synthetic labels\n";
  testMissingFile();
  std::cout << "  [PASS] Missing file error handling\n";
  testExceedsCodeLimit();
  std::cout << "  [PASS] Exceeds code limit error handling\n";
  std::cout << "All disassembler tests passed successfully!\n";
  return 0;
}
