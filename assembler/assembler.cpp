#include "assembler.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

#include "isa_spec/isa201.h"

namespace risc201 {

namespace {

[[maybe_unused]] std::string stripCommentsAndTrim(const std::string& line) {
  const size_t comment = line.find_first_of(";#");
  const std::string clean = (comment == std::string::npos) ? line : line.substr(0, comment);
  const size_t first = clean.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = clean.find_last_not_of(" \t\r\n");
  return clean.substr(first, last - first + 1);
}

[[maybe_unused]] bool isValidIdentifier(const std::string& id) {
  if (id.empty() || (!std::isalpha(static_cast<unsigned char>(id[0])) && id[0] != '_')) {
    return false;
  }
  for (size_t i = 1; i < id.size(); ++i) {
    if (!std::isalnum(static_cast<unsigned char>(id[i])) && id[i] != '_') {
      return false;
    }
  }
  return true;
}

[[maybe_unused]] uint8_t parseRegister(const std::string& token, int line) {
  if (token.size() < 2 || token[0] != 'R') {
    throw std::runtime_error("Line " + std::to_string(line) + ": invalid register '" + token + "'");
  }
  const int idx = (token.size() == 2 && std::isdigit(token[1])) ? (token[1] - '0')
                : (token.size() == 3 && token[1] == '1' && token[2] >= '0' && token[2] <= '5') ? (10 + token[2] - '0')
                : -1;
  if (idx < 0) {
    throw std::runtime_error("Line " + std::to_string(line) + ": register index out of range: " + token);
  }
  return static_cast<uint8_t>(idx);
}

[[maybe_unused]] int32_t parseImmediate(const std::string& token, int line) {
  if (token.empty()) {
    throw std::runtime_error("Line " + std::to_string(line) + ": missing immediate value");
  }
  try {
    size_t idx = 0;
    const int base = (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) ? 16 : 10;
    const long long val = std::stoll(token, &idx, base);
    if (idx != token.size()) {
      throw std::runtime_error("Line " + std::to_string(line) + ": invalid immediate syntax: '" + token + "'");
    }
    if (val < -131072 || val > 131071) {
      if (val >= 0 && val <= 0x3FFFF) {
        return signExtend(static_cast<uint32_t>(val), kImmediateBits);
      }
      throw std::runtime_error("Line " + std::to_string(line) + ": immediate value " + token + " out of 18-bit range");
    }
    return static_cast<int32_t>(val);
  } catch (const std::runtime_error&) {
    throw;
  } catch (...) {
    throw std::runtime_error("Line " + std::to_string(line) + ": invalid immediate operand: '" + token + "'");
  }
}

[[maybe_unused]] std::pair<uint8_t, int32_t> parseMemOperand(const std::string& token, int line) {
  const size_t open = token.find('(');
  if (open == 0 || open == std::string::npos || token.back() != ')') {
    throw std::runtime_error("Line " + std::to_string(line) + ": invalid memory operand syntax '" + token + "', expected imm(RS)");
  }
  const uint8_t rs = parseRegister(token.substr(open + 1, token.size() - open - 2), line);
  const int32_t imm = parseImmediate(token.substr(0, open), line);
  return {rs, imm};
}

}  // namespace

void assemble(const std::string& /*path*/, std::vector<uint32_t>& /*outProgram*/) {
}

void assemble(const std::string& /*path*/) {
}

}  // namespace risc201