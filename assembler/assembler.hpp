#pragma once

// Errors come back as std::runtime_error with the line number in the message.

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

#include "isa_spec/isa201.h"

namespace risc201 {

// Assembles a .s file, populates outProgram, and writes pure machine code to <dir>/<stem>.hex
void assemble(const std::string& path, std::vector<uint32_t>& outProgram);

// Convenience overload that assembles and writes <dir>/<stem>.hex
void assemble(const std::string& path);

}  // namespace risc201

