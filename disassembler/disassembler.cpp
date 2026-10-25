#include "disassembler.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
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

std::string formatInstruction(
    uint32_t word, uint32_t pc = 0, const std::unordered_set<uint32_t>& labelTargets = {},
    bool plain = false) {
  const Opcode op = decodeOpcode(word);
  if (!isDefinedOpcode(op)) {
    std::ostringstream oss;
    oss << "illegal opcode: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<unsigned>(static_cast<uint8_t>(op));
    throw std::runtime_error(oss.str());
  }

  const InstructionFormat fmt = formatOf(op);
  const std::string mnem = mnemonicOf(op);

  switch (fmt) {
    case InstructionFormat::None:
      return mnem;

    case InstructionFormat::R: {
      const auto rs = static_cast<unsigned>(decodeRs(word));
      const auto rt = static_cast<unsigned>(decodeRt(word));
      if (op == Opcode::NOT || op == Opcode::MOV) {
        return mnem + " R" + std::to_string(decodeRd(word)) + ", R" + std::to_string(rs);
      }
      if (op == Opcode::CMP) {
        return mnem + " R" + std::to_string(rs) + ", R" + std::to_string(rt);
      }
      return mnem + " R" + std::to_string(decodeRd(word)) + ", R" + std::to_string(rs) + ", R" +
             std::to_string(rt);
    }

    case InstructionFormat::I: {
      const auto rd = static_cast<unsigned>(decodeRd(word));
      if (op == Opcode::ADDI) {
        return mnem + " R" + std::to_string(rd) + ", R" + std::to_string(decodeRs(word)) + ", " +
               std::to_string(decodeImmediate(word));
      }
      if (op == Opcode::LDI) {
        return mnem + " R" + std::to_string(rd) + ", " + std::to_string(decodeImmediate(word));
      }
      if (op == Opcode::LW || op == Opcode::SW) {
        return mnem + " R" + std::to_string(rd) + ", " + std::to_string(decodeImmediate(word)) + "(R" +
               std::to_string(decodeRs(word)) + ")";
      }
      if (op == Opcode::PUSH || op == Opcode::POP) {
        return mnem + " R" + std::to_string(rd);
      }
      return mnem;
    }

    case InstructionFormat::J: {
      const int32_t offset = decodeOffset(word);
      if (plain) {
        // A single word on its own has no labels, show the raw offset.
        return mnem + " " + std::to_string(offset);
      }
      const int64_t target = static_cast<int64_t>(pc) + 1 + offset;
      if (target >= 0 && labelTargets.count(static_cast<uint32_t>(target)) != 0) {
        return mnem + " L_" + std::to_string(target);
      }
      return mnem + " L_TARGET  ; PC -> PC + " + std::to_string(offset) + ", Branch Target out of bounds";
    }
  }

  throw std::runtime_error("unhandled instruction format");
}

std::string disassembleProgram(const std::vector<uint32_t>& program) {
  std::unordered_set<uint32_t> labelTargets;

  for (uint32_t pc = 0; pc < program.size(); ++pc) {
    const uint32_t word = program[pc];
    const Opcode op = decodeOpcode(word);
    if (!isDefinedOpcode(op)) {
      continue;
    }
    if (formatOf(op) == InstructionFormat::J) {
      const int32_t offset = decodeOffset(word);
      const int64_t target = static_cast<int64_t>(pc) + 1 + offset;
      if (target >= 0 && target < static_cast<int64_t>(program.size())) {
        labelTargets.insert(static_cast<uint32_t>(target));
      }
    }
  }

  std::ostringstream out;
  for (uint32_t pc = 0; pc < program.size(); ++pc) {
    if (labelTargets.count(pc) != 0) {
      out << "L_" << pc << ":\n";
    }
    out << "  " << formatInstruction(program[pc], pc, labelTargets) << "\n";
  }

  return out.str();
}

}  // namespace

std::string disassemble(uint32_t word) {
  const Opcode opcode = decodeOpcode(word);
  if (!isDefinedOpcode(opcode)) {
    std::ostringstream oss;
    oss << "illegal 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<unsigned>(static_cast<uint8_t>(opcode));
    return oss.str();
  }
  return formatInstruction(word, 0, {}, true);
}

void disassemble(const std::string& hex_path) {
  const std::vector<uint32_t> program = readHexFile(hex_path);
  const std::string asmContent = disassembleProgram(program);

  const size_t dot = hex_path.rfind('.');
  const size_t slash = hex_path.rfind('/');
  const std::string asmPath = (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                                  ? hex_path.substr(0, dot) + ".s"
                                  : hex_path + ".s";

  writeAsmFile(asmPath, asmContent);
}

}  // namespace risc201
