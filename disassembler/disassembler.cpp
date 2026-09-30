#include "disassembler.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "isa_spec/isa201.h"

namespace risc201 {

namespace {

std::vector<uint32_t> readHexFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("could not open hex file: " + path);
  }

  std::vector<uint32_t> words;
  std::string line;
  while (std::getline(file, line)) {
    const size_t comment = line.find_first_of(";#");
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
      try {
        words.push_back(static_cast<uint32_t>(std::stoul(token, nullptr, 16)));
      } catch (const std::exception&) {
        throw std::runtime_error("invalid hex word in file: " + token);
      }
    }
  }

  if (words.size() > kCodeLimit + 1) {
    throw std::runtime_error("hex program size (" + std::to_string(words.size()) +
                             " words) exceeds code region limit (" + std::to_string(kCodeLimit + 1) + ")");
  }

  return words;
}

void writeAsmFile(const std::string& path, const std::string& content) {
  std::ofstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("could not open output assembly file: " + path);
  }
  file << content;
}

}  // namespace

void disassemble(const std::string& /*hex_path*/) {
}

}  // namespace risc201
