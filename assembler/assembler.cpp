#include "assembler/assembler.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace risc201 {

namespace {

[[noreturn]] void error(int line, const std::string& msg) {
  throw std::runtime_error("Line " + std::to_string(line) + ": " + msg);
}

std::string trim(const std::string& s) {
  const size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

std::string stripComments(const std::string& line) {
  const size_t commentPos = line.find_first_of(";#");
  const std::string clean = (commentPos == std::string::npos) ? line : line.substr(0, commentPos);
  return trim(clean);
}

bool isValidIdentifier(const std::string& s) {
  if (s.empty()) {
    return false;
  }
  if (!std::isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_') {
    return false;
  }
  for (size_t i = 1; i < s.size(); ++i) {
    if (!std::isalnum(static_cast<unsigned char>(s[i])) && s[i] != '_') {
      return false;
    }
  }
  return true;
}

struct ParsedLine {
  int lineNumber = 0;
  std::string instructionText;
};

uint8_t parseRegister(const std::string& token, int line) {
  if (token.empty() || token[0] != 'R') {
    error(line, "invalid register '" + token + "': registers must be uppercase 'R0'..'R15'");
  }
  const std::string numPart = token.substr(1);
  if (numPart.empty()) {
    error(line, "invalid register '" + token + "'");
  }
  for (char c : numPart) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      error(line, "invalid register '" + token + "'");
    }
  }
  int regNum = -1;
  try {
    regNum = std::stoi(numPart);
  } catch (const std::exception&) {
    error(line, "invalid register '" + token + "'");
  }
  if (regNum < 0 || regNum > 15) {
    error(line, "register index out of range [0..15]: '" + token + "'");
  }
  return static_cast<uint8_t>(regNum);
}

bool tryParseInteger(const std::string& token, int32_t& outVal) {
  if (token.empty()) {
    return false;
  }
  size_t idx = 0;
  bool negative = false;
  if (token[idx] == '+' || token[idx] == '-') {
    if (token[idx] == '-') {
      negative = true;
    }
    idx++;
  }
  if (idx >= token.size()) {
    return false;
  }
  int base = 10;
  if (token.compare(idx, 2, "0x") == 0 || token.compare(idx, 2, "0X") == 0) {
    base = 16;
    idx += 2;
    if (idx >= token.size()) {
      return false;
    }
  }
  for (size_t i = idx; i < token.size(); ++i) {
    const char c = token[i];
    if (base == 10 && !std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    if (base == 16 && !std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  try {
    long long val = std::stoll(token.substr(negative ? 1 : 0), nullptr, base);
    if (negative) {
      val = -val;
    }
    outVal = static_cast<int32_t>(val);
    return true;
  } catch (...) {
    return false;
  }
}

int32_t parseImmediate(const std::string& token, int line,
                       const std::unordered_map<std::string, uint32_t>& symbols) {
  int32_t val = 0;
  if (tryParseInteger(token, val)) {
    if (val < -131072 || val > 131071) {
      error(line, "immediate value out of 18-bit signed range [-131072, 131071]: " + token);
    }
    return val;
  }

  const auto it = symbols.find(token);
  if (it != symbols.end()) {
    const int32_t addr = static_cast<int32_t>(it->second);
    if (addr < -131072 || addr > 131071) {
      error(line, "symbol address out of 18-bit signed range: " + token);
    }
    return addr;
  }

  error(line, "invalid immediate or undefined symbol '" + token + "'");
}

int32_t parseTarget(const std::string& token, int line,
                    const std::unordered_map<std::string, uint32_t>& symbols, uint32_t currentPC) {
  int32_t litVal = 0;
  if (tryParseInteger(token, litVal)) {
    if (litVal < -33554432 || litVal > 33554431) {
      error(line, "jump offset out of 26-bit signed range: " + token);
    }
    return litVal;
  }

  const auto it = symbols.find(token);
  if (it == symbols.end()) {
    error(line, "undefined label '" + token + "'");
  }

  const uint32_t targetAddr = it->second;
  const int32_t offset = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(currentPC + 1);
  if (offset < -33554432 || offset > 33554431) {
    error(line, "computed jump offset out of 26-bit range for label '" + token + "'");
  }
  return offset;
}

std::pair<uint8_t, int32_t> parseMemOperand(
    const std::string& token, int line, const std::unordered_map<std::string, uint32_t>& symbols) {
  const size_t openParen = token.find('(');
  const size_t closeParen = token.find(')');
  if (openParen == std::string::npos || closeParen != token.size() - 1 || openParen >= closeParen) {
    error(line, "malformed memory operand '" + token + "'; expected 'imm(RS)'");
  }

  const std::string immStr = trim(token.substr(0, openParen));
  const std::string regStr = trim(token.substr(openParen + 1, closeParen - openParen - 1));

  const uint8_t rs = parseRegister(regStr, line);
  const int32_t imm = immStr.empty() ? 0 : parseImmediate(immStr, line, symbols);
  return {rs, imm};
}

std::vector<std::string> splitOperands(const std::string& operandsStr, int line) {
  std::vector<std::string> operands;
  std::string current;
  int parenDepth = 0;

  for (char c : operandsStr) {
    if (c == '(') {
      parenDepth++;
      current += c;
    } else if (c == ')') {
      parenDepth--;
      current += c;
    } else if (c == ',' && parenDepth == 0) {
      std::string trimmed = trim(current);
      if (trimmed.empty()) {
        error(line, "unexpected empty operand before comma");
      }
      operands.push_back(trimmed);
      current.clear();
    } else {
      current += c;
    }
  }

  if (parenDepth != 0) {
    error(line, "unmatched parentheses in operands: '" + operandsStr + "'");
  }

  const std::string trimmed = trim(current);
  if (!trimmed.empty()) {
    operands.push_back(trimmed);
  } else if (!operands.empty()) {
    error(line, "trailing comma with missing operand");
  }

  return operands;
}

const InstructionInfo* findInstructionByMnemonic(const std::string& mnemonic) {
  for (std::size_t i = 0; i < kInstructionCount; ++i) {
    if (mnemonic == kInstructionTable[i].mnemonic) {
      return &kInstructionTable[i];
    }
  }
  return nullptr;
}

}  // namespace

void assemble(const std::string& path, std::vector<uint32_t>& outProgram) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("could not open source file: " + path);
  }

  outProgram.clear();

  // Pass 1: Label collection and instruction line identification
  std::unordered_map<std::string, uint32_t> symbols;
  std::vector<ParsedLine> parsedLines;
  uint32_t instructionCount = 0;
  std::string rawLine;
  int currentLineNum = 0;

  while (std::getline(file, rawLine)) {
    currentLineNum++;
    std::string clean = stripComments(rawLine);
    if (clean.empty()) {
      continue;
    }

    const size_t colonPos = clean.find(':');
    if (colonPos != std::string::npos) {
      const std::string label = trim(clean.substr(0, colonPos));
      if (!isValidIdentifier(label)) {
        error(currentLineNum, "invalid label identifier '" + label + "'");
      }
      if (symbols.find(label) != symbols.end()) {
        error(currentLineNum, "duplicate label definition '" + label + "'");
      }
      symbols[label] = instructionCount;

      const std::string rest = trim(clean.substr(colonPos + 1));
      if (!rest.empty()) {
        parsedLines.push_back({currentLineNum, rest});
        instructionCount++;
      }
    } else {
      parsedLines.push_back({currentLineNum, clean});
      instructionCount++;
    }
  }

  if (instructionCount > kCodeLimit + 1) {
    throw std::runtime_error("program size (" + std::to_string(instructionCount) +
                             " words) exceeds code region limit (" +
                             std::to_string(kCodeLimit + 1) + " words)");
  }

  // Pass 2: Parsing instructions and encoding words
  outProgram.reserve(instructionCount);

  for (size_t i = 0; i < parsedLines.size(); ++i) {
    const int line = parsedLines[i].lineNumber;
    const std::string& text = parsedLines[i].instructionText;
    const uint32_t pc = static_cast<uint32_t>(i);

    const size_t firstSpace = text.find_first_of(" \t");
    const std::string mnemonic = (firstSpace == std::string::npos) ? text : text.substr(0, firstSpace);
    const std::string operandsStr = (firstSpace == std::string::npos) ? "" : trim(text.substr(firstSpace + 1));

    for (char c : mnemonic) {
      if (std::isalpha(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(c))) {
        error(line, "mnemonic must be strictly uppercase: '" + mnemonic + "'");
      }
    }

    const InstructionInfo* info = findInstructionByMnemonic(mnemonic);
    if (!info) {
      error(line, "unknown instruction mnemonic '" + mnemonic + "'");
    }

    const std::vector<std::string> operands = splitOperands(operandsStr, line);
    uint32_t encodedWord = 0;

    switch (info->format) {
      case InstructionFormat::None: {
        if (!operands.empty()) {
          error(line, "'" + mnemonic + "' takes no operands, found: " + operandsStr);
        }
        encodedWord = encodeOpcode(info->opcode);
        break;
      }

      case InstructionFormat::R: {
        if (info->opcode == Opcode::NOT || info->opcode == Opcode::MOV) {
          if (operands.size() != 2) {
            error(line, "'" + mnemonic + "' takes exactly 2 operands: RD, RS");
          }
          const uint8_t rd = parseRegister(operands[0], line);
          const uint8_t rs = parseRegister(operands[1], line);
          encodedWord = encodeR(info->opcode, rd, rs, 0);
        } else if (info->opcode == Opcode::CMP) {
          if (operands.size() != 2) {
            error(line, "CMP takes exactly 2 operands: RS, RT");
          }
          const uint8_t rs = parseRegister(operands[0], line);
          const uint8_t rt = parseRegister(operands[1], line);
          encodedWord = encodeR(Opcode::CMP, 0, rs, rt);
        } else {
          if (operands.size() != 3) {
            error(line, "'" + mnemonic + "' takes exactly 3 operands: RD, RS, RT");
          }
          const uint8_t rd = parseRegister(operands[0], line);
          const uint8_t rs = parseRegister(operands[1], line);
          const uint8_t rt = parseRegister(operands[2], line);
          encodedWord = encodeR(info->opcode, rd, rs, rt);
        }
        break;
      }

      case InstructionFormat::I: {
        if (info->opcode == Opcode::ADDI) {
          if (operands.size() != 3) {
            error(line, "ADDI takes exactly 3 operands: RD, RS, imm");
          }
          const uint8_t rd = parseRegister(operands[0], line);
          const uint8_t rs = parseRegister(operands[1], line);
          const int32_t imm = parseImmediate(operands[2], line, symbols);
          encodedWord = encodeI(Opcode::ADDI, rd, rs, imm);
        } else if (info->opcode == Opcode::LDI) {
          if (operands.size() != 2) {
            error(line, "LDI takes exactly 2 operands: RD, imm");
          }
          const uint8_t rd = parseRegister(operands[0], line);
          const int32_t imm = parseImmediate(operands[1], line, symbols);
          encodedWord = encodeI(Opcode::LDI, rd, 0, imm);
        } else if (info->opcode == Opcode::LW || info->opcode == Opcode::SW) {
          if (operands.size() != 2) {
            error(line, "'" + mnemonic + "' takes exactly 2 operands: RD, imm(RS)");
          }
          const uint8_t rd = parseRegister(operands[0], line);
          const auto [rs, imm] = parseMemOperand(operands[1], line, symbols);
          encodedWord = encodeI(info->opcode, rd, rs, imm);
        } else if (info->opcode == Opcode::PUSH || info->opcode == Opcode::POP) {
          if (operands.size() != 1) {
            error(line, "'" + mnemonic + "' takes exactly 1 operand: RD");
          }
          const uint8_t rd = parseRegister(operands[0], line);
          encodedWord = encodeI(info->opcode, rd, 0, 0);
        } else if (info->opcode == Opcode::RET) {
          if (!operands.empty()) {
            error(line, "RET takes no operands");
          }
          encodedWord = encodeI(Opcode::RET, 0, 0, 0);
        }
        break;
      }

      case InstructionFormat::J: {
        if (operands.size() != 1) {
          error(line, "'" + mnemonic + "' takes exactly 1 target operand (offset or label)");
        }
        const int32_t offset = parseTarget(operands[0], line, symbols, pc);
        encodedWord = encodeJ(info->opcode, offset);
        break;
      }
    }

    outProgram.push_back(encodedWord);
  }
}

void assemble(const std::string& path) {
  std::vector<uint32_t> program;
  assemble(path, program);
}

}  // namespace risc201
