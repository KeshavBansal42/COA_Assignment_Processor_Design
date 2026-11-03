#include "assembler.hpp"

#include "assembler/preprocessor.hpp"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "isa_spec/isa201.h"

namespace risc201 {

namespace {

using SymbolTable = std::unordered_map<std::string, uint32_t>;

std::string stripCommentsAndTrim(const std::string& line) {
  const size_t comment = line.find_first_of(";#");
  const std::string clean = (comment == std::string::npos) ? line : line.substr(0, comment);
  const size_t first = clean.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = clean.find_last_not_of(" \t\r\n");
  return clean.substr(first, last - first + 1);
}

bool isValidIdentifier(const std::string& id) {
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

uint8_t parseRegister(const std::string& token, int line) {
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

int32_t parseImmediate(const std::string& token, int line) {
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

std::pair<uint8_t, int32_t> parseMemOperand(const std::string& token, int line) {
  const size_t open = token.find('(');
  if (open == 0 || open == std::string::npos || token.back() != ')') {
    throw std::runtime_error("Line " + std::to_string(line) + ": invalid memory operand syntax '" + token + "', expected imm(RS)");
  }
  const uint8_t rs = parseRegister(token.substr(open + 1, token.size() - open - 2), line);
  const int32_t imm = parseImmediate(token.substr(0, open), line);
  return {rs, imm};
}

int32_t parseBranchTarget(const std::string& token, int line, const SymbolTable& symbols, uint32_t pc) {
  if (token.empty()) {
    throw std::runtime_error("Line " + std::to_string(line) + ": missing branch/jump target");
  }
  if (token[0] == '+' || token[0] == '-' || std::isdigit(static_cast<unsigned char>(token[0]))) {
    throw std::runtime_error("Line " + std::to_string(line) + ": manual PC offset not allowed, target must be a label: '" + token + "'");
  }
  const auto it = symbols.find(token);
  if (it == symbols.end()) {
    throw std::runtime_error("Line " + std::to_string(line) + ": undefined label '" + token + "'");
  }
  const int64_t offset = static_cast<int64_t>(it->second) - static_cast<int64_t>(pc + 1);
  if (offset < -33554432 || offset > 33554431) {
    throw std::runtime_error("Line " + std::to_string(line) + ": branch target offset out of range: '" + token + "'");
  }
  return static_cast<int32_t>(offset);
}

const InstructionInfo* findInstruction(const std::string& mnemonic) {
  for (size_t i = 0; i < kInstructionCount; ++i) {
    if (mnemonic == kInstructionTable[i].mnemonic) {
      return &kInstructionTable[i];
    }
  }
  return nullptr;
}

std::vector<std::string> splitOperands(const std::string& str, int line) {
  std::vector<std::string> ops;
  if (str.empty()) return ops;
  std::stringstream ss(str);
  std::string item;
  while (std::getline(ss, item, ',')) {
    const size_t first = item.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      throw std::runtime_error("Line " + std::to_string(line) + ": empty operand between commas");
    }
    const size_t last = item.find_last_not_of(" \t\r\n");
    ops.push_back(item.substr(first, last - first + 1));
  }
  return ops;
}

uint32_t encodeInstruction(const std::string& mnemonic, const std::string& operandsStr, int line, const SymbolTable& symbols, uint32_t pc) {
  for (char c : mnemonic) {
    if (std::isalpha(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(c))) {
      throw std::runtime_error("Line " + std::to_string(line) + ": mnemonic must be strictly uppercase: '" + mnemonic + "'");
    }
  }

  const InstructionInfo* info = findInstruction(mnemonic);
  if (!info) {
    throw std::runtime_error("Line " + std::to_string(line) + ": unknown instruction mnemonic '" + mnemonic + "'");
  }

  const std::vector<std::string> ops = splitOperands(operandsStr, line);

  switch (info->format) {
    case InstructionFormat::None: {
      if (!ops.empty()) {
        throw std::runtime_error("Line " + std::to_string(line) + ": '" + mnemonic + "' takes no operands");
      }
      return encodeOpcode(info->opcode);
    }
    case InstructionFormat::R: {
      if (info->opcode == Opcode::NOT || info->opcode == Opcode::MOV) {
        if (ops.size() != 2) {
          throw std::runtime_error("Line " + std::to_string(line) + ": '" + mnemonic + "' takes exactly 2 operands: RD, RS");
        }
        return encodeR(info->opcode, parseRegister(ops[0], line), parseRegister(ops[1], line), 0);
      }
      if (info->opcode == Opcode::CMP) {
        if (ops.size() != 2) {
          throw std::runtime_error("Line " + std::to_string(line) + ": CMP takes exactly 2 operands: RS, RT");
        }
        return encodeR(Opcode::CMP, 0, parseRegister(ops[0], line), parseRegister(ops[1], line));
      }
      if (ops.size() != 3) {
        throw std::runtime_error("Line " + std::to_string(line) + ": '" + mnemonic + "' takes exactly 3 operands: RD, RS, RT");
      }
      return encodeR(info->opcode, parseRegister(ops[0], line), parseRegister(ops[1], line), parseRegister(ops[2], line));
    }
    case InstructionFormat::I: {
      if (info->opcode == Opcode::ADDI) {
        if (ops.size() != 3) {
          throw std::runtime_error("Line " + std::to_string(line) + ": ADDI takes exactly 3 operands: RD, RS, imm");
        }
        return encodeI(Opcode::ADDI, parseRegister(ops[0], line), parseRegister(ops[1], line), parseImmediate(ops[2], line));
      }
      if (info->opcode == Opcode::LDI) {
        if (ops.size() != 2) {
          throw std::runtime_error("Line " + std::to_string(line) + ": LDI takes exactly 2 operands: RD, imm");
        }
        return encodeI(Opcode::LDI, parseRegister(ops[0], line), 0, parseImmediate(ops[1], line));
      }
      if (info->opcode == Opcode::LW || info->opcode == Opcode::SW) {
        if (ops.size() != 2) {
          throw std::runtime_error("Line " + std::to_string(line) + ": '" + mnemonic + "' takes exactly 2 operands: RD, imm(RS)");
        }
        const uint8_t rd = parseRegister(ops[0], line);
        const auto [rs, imm] = parseMemOperand(ops[1], line);
        return encodeI(info->opcode, rd, rs, imm);
      }
      if (info->opcode == Opcode::PUSH || info->opcode == Opcode::POP) {
        if (ops.size() != 1) {
          throw std::runtime_error("Line " + std::to_string(line) + ": '" + mnemonic + "' takes exactly 1 operand: RD");
        }
        return encodeI(info->opcode, parseRegister(ops[0], line), 0, 0);
      }
      if (info->opcode == Opcode::RET) {
        if (!ops.empty()) {
          throw std::runtime_error("Line " + std::to_string(line) + ": RET takes no operands");
        }
        return encodeI(Opcode::RET, 0, 0, 0);
      }
      throw std::runtime_error("Line " + std::to_string(line) + ": unhandled I-type instruction");
    }
    case InstructionFormat::J: {
      if (ops.size() != 1) {
        throw std::runtime_error("Line " + std::to_string(line) + ": '" + mnemonic + "' takes exactly 1 target operand");
      }
      return encodeJ(info->opcode, parseBranchTarget(ops[0], line, symbols, pc));
    }
  }
  throw std::runtime_error("Line " + std::to_string(line) + ": invalid instruction format");
}

void writeHexFile(const std::string& hexPath, const std::vector<uint32_t>& program) {
  std::ofstream out(hexPath);
  if (!out.is_open()) {
    throw std::runtime_error("could not open output hex file: " + hexPath);
  }
  for (uint32_t word : program) {
    out << std::hex << std::setw(8) << std::setfill('0') << word << "\n";
  }
}

}  // namespace

void assemble(const std::string& path, std::vector<uint32_t>& outProgram) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("could not open source file: " + path);
  }

  // Read the source, then run the stack macro preprocessor so the two passes
  // below only ever see flat instructions.
  std::vector<SourceLine> source;
  std::string raw;
  int lineNum = 0;
  while (std::getline(file, raw)) {
    lineNum++;
    source.push_back({raw, lineNum});
  }
  const std::vector<SourceLine> expanded = preprocess(source);

  SymbolTable symbols;
  struct LineItem {
    int lineNum;
    std::string text;
  };
  std::vector<LineItem> instructions;

  // Pass 1: Standalone labels & instruction collection
  for (const SourceLine& item : expanded) {
    const std::string line = stripCommentsAndTrim(item.text);
    if (line.empty()) continue;

    const size_t colon = line.find(':');
    if (colon != std::string::npos) {
      if (colon != line.size() - 1) {
        throw std::runtime_error("Line " + std::to_string(item.line) + ": inline labels not allowed, label must be on its own line");
      }
      const std::string label = line.substr(0, colon);
      if (!isValidIdentifier(label)) {
        throw std::runtime_error("Line " + std::to_string(item.line) + ": invalid label identifier '" + label + "'");
      }
      if (symbols.find(label) != symbols.end()) {
        throw std::runtime_error("Line " + std::to_string(item.line) + ": duplicate label '" + label + "'");
      }
      symbols[label] = static_cast<uint32_t>(instructions.size());
    } else {
      instructions.push_back({item.line, line});
    }
  }

  if (instructions.size() > kCodeLimit + 1) {
    throw std::runtime_error("program size (" + std::to_string(instructions.size()) +
                             " words) exceeds code region limit (" + std::to_string(kCodeLimit + 1) + " words)");
  }

  // Pass 2: Encoding
  outProgram.clear();
  outProgram.reserve(instructions.size());

  for (size_t pc = 0; pc < instructions.size(); ++pc) {
    const std::string& text = instructions[pc].text;
    const int line = instructions[pc].lineNum;
    const size_t space = text.find_first_of(" \t");
    const std::string mnemonic = (space == std::string::npos) ? text : text.substr(0, space);
    const std::string operandsStr = (space == std::string::npos) ? "" : stripCommentsAndTrim(text.substr(space + 1));

    outProgram.push_back(encodeInstruction(mnemonic, operandsStr, line, symbols, static_cast<uint32_t>(pc)));
  }

  // Inline hex path derivation
  const size_t dot = path.rfind('.');
  const size_t slash = path.rfind('/');
  const std::string hexPath = (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                                  ? path.substr(0, dot) + ".hex"
                                  : path + ".hex";
  writeHexFile(hexPath, outProgram);
}

void assemble(const std::string& path) {
  std::vector<uint32_t> program;
  assemble(path, program);
}

}  // namespace risc201