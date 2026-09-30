#pragma once

#include <string>

namespace risc201 {

// Disassembles a .hex file into a formatted .s assembly file in the same directory.
void disassemble(const std::string& hex_path);

}  // namespace risc201
