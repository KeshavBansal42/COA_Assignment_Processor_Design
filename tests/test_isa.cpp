// Encoding and decoding checks for the frozen ISA header.

#include <cassert>
#include <cstdint>

#include "isa_spec/isa201.h"

using namespace risc201;

static void testRTypeFields() {
  const uint32_t word = encodeR(Opcode::ADD, 3, 5, 7);
  assert(decodeOpcode(word) == Opcode::ADD);
  assert(decodeRd(word) == 3);
  assert(decodeRs(word) == 5);
  assert(decodeRt(word) == 7);
}

static void testITypeFields() {
  const uint32_t word = encodeI(Opcode::ADDI, 1, 2, -5);
  assert(decodeOpcode(word) == Opcode::ADDI);
  assert(decodeRd(word) == 1);
  assert(decodeRs(word) == 2);
  assert(decodeImmediate(word) == -5);
}

static void testImmediateLimits() {
  // 18 signed bits, so the range is -131072 to 131071.
  assert(decodeImmediate(encodeI(Opcode::LDI, 0, 0, 131071)) == 131071);
  assert(decodeImmediate(encodeI(Opcode::LDI, 0, 0, -131072)) == -131072);
}

static void testJTypeOffset() {
  const uint32_t word = encodeJ(Opcode::JMP, -3);
  assert(decodeOpcode(word) == Opcode::JMP);
  assert(decodeOffset(word) == -3);
}

static void testOpcodeTable() {
  assert(kInstructionCount == 27);
  assert(isDefinedOpcode(Opcode::CALL));
  assert(!isDefinedOpcode(static_cast<Opcode>(0x3F)));
  assert(formatOf(Opcode::LW) == InstructionFormat::I);
  assert(formatOf(Opcode::BEQ) == InstructionFormat::J);
}

int main() {
  testRTypeFields();
  testITypeFields();
  testImmediateLimits();
  testJTypeOffset();
  testOpcodeTable();
  return 0;
}
