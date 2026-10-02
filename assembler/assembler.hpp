#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace risc201 {

void assemble(const std::string& path, std::vector<uint32_t>& outProgram);
void assemble(const std::string& path);

}  // namespace risc201