#include "simulator_core/cli.hpp"
#include "memory_safety/memory_safety.h"

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

Cli::Cli(Cpu& cpu, std::ostream& out) : cpu_(cpu), debugger_(cpu), out_(out) {}

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
  } else if (command == "run" || command == "continue") {
    commandRun();
  } else if (command == "step") {
    int count = 1;
    if (!(input >> count)) {
      count = 1;
    }
    commandStep(count);
  } else if (command == "reset") {
    commandReset();
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
  } else if (command == "break") {
    std::string target;
    if (input >> target) {
      commandBreak(target);
    } else {
      out_ << "usage: break <address|label>\n";
    }
  } else if (command == "delete") {
    std::string target;
    if (input >> target) {
      commandDelete(target);
    } else {
      out_ << "usage: delete <address|label>\n";
    }
  } else if (command == "breaks") {
    commandBreaks();
  } else if (command == "watch") {
    std::string target;
    if (input >> target) {
      commandWatch(target);
    } else {
      out_ << "usage: watch <address|label>\n";
    }
  } else if (command == "unwatch") {
    std::string target;
    if (input >> target) {
      commandUnwatch(target);
    } else {
      out_ << "usage: unwatch <address|label>\n";
    }
  } else if (command == "watches") {
    commandWatches();
  } else if (command == "symbols") {
    std::string path;
    if (input >> path) {
      commandSymbols(path);
    } else {
      out_ << "usage: symbols <file>\n";
    }
  } 
  
  //added after memory_safety part
  else if (command == "stack") {
    int depth = 16;
    if (!(input >> depth)) {
      depth = 16; // default depth if user doesnt provide one
    }
    commandStack(depth);
  } 
  
  else if (command == "help") {
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
  debugger_.reset();
  out_ << "loaded " << program.size() << " words\n";
}

void Cli::commandRun() {
  debugger_.continueRun();
  printStop();
}

void Cli::commandStep(int count) {
  StopReason reason = StopReason::Running;
  for (int i = 0; i < count; ++i) {
    reason = debugger_.stepOnce();
    if (reason != StopReason::Running) {
      break;
    }
  }
  printStop();
}

void Cli::commandReset() {
  debugger_.reset();
  out_ << "reset\n";
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

//added after memory_safety
void Cli::commandStack(int maxDepth) const {
  // run our ascii visualizer
  renderStackAscii(cpu_.rawMemory(), cpu_.sp(), maxDepth);
}

void Cli::commandBreak(const std::string& target) {
  uint32_t address = 0;
  std::string error;
  if (!resolveAddress(target, address, error)) {
    out_ << error << "\n";
    return;
  }

  debugger_.addBreakpoint(address);
  out_ << "breakpoint" << describe(address) << "\n";
}

void Cli::commandDelete(const std::string& target) {
  uint32_t address = 0;
  std::string error;
  if (!resolveAddress(target, address, error)) {
    out_ << error << "\n";
    return;
  }

  if (debugger_.removeBreakpoint(address)) {
    out_ << "deleted breakpoint" << describe(address) << "\n";
  } else {
    out_ << "no breakpoint" << describe(address) << "\n";
  }
}

void Cli::commandBreaks() const {
  if (debugger_.breakpoints().empty()) {
    out_ << "no breakpoints\n";
    return;
  }

  for (const uint32_t address : debugger_.breakpoints()) {
    out_ << "breakpoint" << describe(address) << "\n";
  }
}

void Cli::commandWatch(const std::string& target) {
  uint32_t address = 0;
  std::string error;
  if (!resolveAddress(target, address, error)) {
    out_ << error << "\n";
    return;
  }

  debugger_.addWatchpoint(address);
  out_ << "watchpoint" << describe(address) << "\n";
}

void Cli::commandUnwatch(const std::string& target) {
  uint32_t address = 0;
  std::string error;
  if (!resolveAddress(target, address, error)) {
    out_ << error << "\n";
    return;
  }

  if (debugger_.removeWatchpoint(address)) {
    out_ << "removed watchpoint" << describe(address) << "\n";
  } else {
    out_ << "no watchpoint" << describe(address) << "\n";
  }
}

void Cli::commandWatches() const {
  if (debugger_.watchpoints().empty()) {
    out_ << "no watchpoints\n";
    return;
  }

  for (const uint32_t address : debugger_.watchpoints()) {
    out_ << "watchpoint" << describe(address) << "\n";
  }
}

void Cli::commandSymbols(const std::string& path) {
  std::string error;
  if (!debugger_.loadSymbols(path, error)) {
    out_ << error << "\n";
    return;
  }
  out_ << "symbols loaded from " << path << "\n";
}

bool Cli::resolveAddress(const std::string& text, uint32_t& address, std::string& error) const {
  if (parseNumber(text, address)) {
    return true;
  }
  if (debugger_.findSymbol(text, address)) {
    return true;
  }
  error = "unknown address or label " + text;
  return false;
}

std::string Cli::describe(uint32_t address) const {
  std::string text = " at 0x" + toHex(address);
  const std::string name = debugger_.symbolFor(address);
  if (!name.empty()) {
    text += " (" + name + ")";
  }
  return text;
}

void Cli::printStop() const {
  const uint32_t address = debugger_.stopAddress();

  if (debugger_.stopReason() == StopReason::Breakpoint) {
    out_ << "breakpoint" << describe(address) << "\n";
    return;
  }

  if (debugger_.stopReason() == StopReason::Watchpoint) {
    out_ << "watchpoint" << describe(address) << " now 0x" << toHex(cpu_.readMem(address))
         << "\n";
    return;
  }

  printStatus();
}

void Cli::printStatus() const {
  if (cpu_.exception() != ExceptionCode::NONE) {
    out_ << "exception " << exceptionToString(cpu_.exception()) << " at PC 0x" << toHex(cpu_.pc())
         << "\n";
  } else if (cpu_.halted()) {
    out_ << "halted at PC 0x" << toHex(cpu_.pc()) << "\n";
  } else {
    out_ << "stopped at PC 0x" << toHex(cpu_.pc()) << "\n";
  }
}

void Cli::printHelp() const {
  out_ << "load <file>         load a program, one hex word per line\n";
  out_ << "symbols <file>      load labels, one \"name address\" per line\n";
  out_ << "run                 run until a breakpoint, watchpoint, HALT or an exception\n";
  out_ << "step [count]        run count instructions, default 1\n";
  out_ << "reset               reset the CPU, the program stays loaded\n";
  out_ << "regs                show the registers, PC, SP and FLAGS\n";
  out_ << "mem [start] [n]     show n words from start, default 0x000 and 16\n";
  out_ << "break <addr|label>  stop before the instruction at the address\n";
  out_ << "delete <addr|label> remove a breakpoint\n";
  out_ << "breaks              list the breakpoints\n";
  out_ << "watch <addr|label>  stop when the address is written\n";
  out_ << "unwatch <addr|label> remove a watchpoint\n";
  out_ << "watches             list the watchpoints\n";
  out_ << "help                show this list\n";
  out_ << "quit                leave the simulator\n";
}

}  // namespace risc201
