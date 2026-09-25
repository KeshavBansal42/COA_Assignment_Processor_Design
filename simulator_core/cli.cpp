#include "simulator_core/cli.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace risc201 {

namespace {

std::string toHex(uint32_t value) {
  std::ostringstream text;
  text << std::hex << std::setw(8) << std::setfill('0') << value;
  return text.str();
}

// Accepts decimal and 0x prefixed hex.
bool parseNumber(const std::string& text, uint32_t& value) {
  try {
    std::size_t used = 0;
    const unsigned long parsed = std::stoul(text, &used, 0);
    if (used != text.size() || parsed > 0xFFFFFFFFul) {
      return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
  } catch (const std::invalid_argument&) {
    return false;
  } catch (const std::out_of_range&) {
    return false;
  }
}

}  // namespace

Cli::Cli(Cpu& cpu, std::ostream& out) : cpu_(cpu), out_(out) {}

bool Cli::execute(const std::string& line) {
  std::istringstream input(line);
  std::string command;
  if (!(input >> command)) {
    return true;
  }

  if (command == "load") {
    std::string path;
    if (input >> path) {
      commandLoad(path);
    } else {
      out_ << "usage: load <file>\n";
    }
  } else if (command == "run") {
    commandRun();
  } else if (command == "step") {
    int count = 1;
    if (!(input >> count)) {
      count = 1;
    }
    commandStep(count);
  } else if (command == "regs") {
    commandRegs();
  } else if (command == "mem") {
    std::string startText;
    std::string countText;
    uint32_t start = kCodeBase;
    uint32_t count = 16;
    if (input >> startText && !parseNumber(startText, start)) {
      out_ << "bad address " << startText << "\n";
      return true;
    }
    if (input >> countText && !parseNumber(countText, count)) {
      out_ << "bad count " << countText << "\n";
      return true;
    }
    commandMem(start, count);
  } else if (command == "help") {
    printHelp();
  } else if (command == "quit" || command == "exit") {
    return false;
  } else {
    out_ << "unknown command " << command << ", type help\n";
  }

  return true;
}

void Cli::commandLoad(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    out_ << "cannot open " << path << "\n";
    return;
  }

  std::vector<uint32_t> program;
  std::string line;
  while (std::getline(file, line)) {
    const std::size_t comment = line.find_first_of("#;");
    if (comment != std::string::npos) {
      line.erase(comment);
    }

    std::istringstream words(line);
    std::string word;
    while (words >> word) {
      try {
        program.push_back(static_cast<uint32_t>(std::stoul(word, nullptr, 16)));
      } catch (const std::exception&) {
        out_ << "bad word " << word << "\n";
        return;
      }
    }
  }

  if (program.empty()) {
    out_ << path << " has no words\n";
    return;
  }

  if (program.size() > kCodeLimit + 1) {
    out_ << "program does not fit in the code region\n";
    return;
  }

  cpu_.load(program);
  out_ << "loaded " << program.size() << " words\n";
}

void Cli::commandRun() {
  cpu_.run();
  printStatus();
}

void Cli::commandStep(int count) {
  for (int i = 0; i < count; ++i) {
    if (cpu_.halted() || cpu_.exception() != ExceptionCode::NONE) {
      break;
    }
    cpu_.step();
  }
  printStatus();
}

void Cli::commandRegs() const {
  for (std::size_t i = 0; i < kRegisterCount; ++i) {
    out_ << "r" << i << " = 0x" << toHex(cpu_.reg(static_cast<uint8_t>(i)));
    out_ << (i % 4 == 3 ? "\n" : "  ");
  }

  out_ << "PC = 0x" << toHex(cpu_.pc()) << "  SP = 0x" << toHex(cpu_.sp()) << "\n";

  const Flags flags = cpu_.flags();
  out_ << "FLAGS Z=" << flags.z << " N=" << flags.n << " C=" << flags.c << " V=" << flags.v
       << "\n";
}

void Cli::commandMem(uint32_t start, uint32_t count) const {
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t address = start + i;
    if (address > kLastAddress) {
      out_ << "end of memory at 0x" << toHex(kLastAddress) << "\n";
      return;
    }
    if (i % 8 == 0) {
      out_ << "0x" << toHex(address) << ":";
    }
    out_ << " " << toHex(cpu_.readMem(address));
    if (i % 8 == 7) {
      out_ << "\n";
    }
  }

  if (count % 8 != 0) {
    out_ << "\n";
  }
}

void Cli::printStatus() const {
  if (cpu_.exception() != ExceptionCode::NONE) {
    out_ << "exception " << toString(cpu_.exception()) << " at PC 0x" << toHex(cpu_.pc())
         << "\n";
  } else if (cpu_.halted()) {
    out_ << "halted at PC 0x" << toHex(cpu_.pc()) << "\n";
  } else {
    out_ << "stopped at PC 0x" << toHex(cpu_.pc()) << "\n";
  }
}

void Cli::printHelp() const {
  out_ << "load <file>        load a program, one hex word per line\n";
  out_ << "run                run until HALT or an exception\n";
  out_ << "step [count]       run count instructions, default 1\n";
  out_ << "regs               show the registers, PC, SP and FLAGS\n";
  out_ << "mem [start] [n]    show n words from start, default 0x000 and 16\n";
  out_ << "help               show this list\n";
  out_ << "quit               leave the simulator\n";
}

}  // namespace risc201
