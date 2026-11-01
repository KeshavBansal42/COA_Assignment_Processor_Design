#pragma once

#include <cstdint>
#include <string>

namespace risc201 {

// Formats one instruction word on its own, with the raw branch offset instead
// of a label. Never throws: an undefined opcode comes back as "illegal 0xNN".
std::string disassemble(uint32_t word);

// Disassembles a .hex file into a formatted .s assembly file in the same directory.
void disassemble(const std::string& hex_path);

}  // namespace risc201
