#include "simulator_core/debugger.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace risc201 {

namespace {

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

Debugger::Debugger(Cpu& cpu) : cpu_(cpu) {}

StopReason Debugger::stop(StopReason reason, uint32_t address) {
  stopReason_ = reason;
  stopAddress_ = address;
  return reason;
}

StopReason Debugger::stepOnce() {
  cpu_.step();

  if (cpu_.exception() != ExceptionCode::NONE) {
    return stop(StopReason::Exception, cpu_.pc());
  }
  if (cpu_.halted()) {
    return stop(StopReason::Halted, cpu_.pc());
  }
  if (cpu_.wroteMemory() && hasWatchpoint(cpu_.lastWriteAddress())) {
    return stop(StopReason::Watchpoint, cpu_.lastWriteAddress());
  }
  return stop(StopReason::Running, cpu_.pc());
}

StopReason Debugger::continueRun() {
  StopReason reason = StopReason::Running;

  // A breakpoint stops before its instruction, so continuing from one runs that
  // instruction before the breakpoint can fire again.
  bool skipCurrent = resumeFromBreakpoint_ && hasBreakpoint(cpu_.pc());
  resumeFromBreakpoint_ = false;

  while (reason == StopReason::Running) {
    if (cpu_.halted() || cpu_.exception() != ExceptionCode::NONE) {
      break;
    }
    if (!skipCurrent && hasBreakpoint(cpu_.pc())) {
      resumeFromBreakpoint_ = true;
      return stop(StopReason::Breakpoint, cpu_.pc());
    }
    skipCurrent = false;
    reason = stepOnce();
  }

  return reason;
}

StopReason Debugger::stopReason() const { return stopReason_; }

uint32_t Debugger::stopAddress() const { return stopAddress_; }

void Debugger::addBreakpoint(uint32_t address) {
  if (!hasBreakpoint(address)) {
    breakpoints_.push_back(address);
  }
}

bool Debugger::removeBreakpoint(uint32_t address) {
  for (std::vector<uint32_t>::iterator it = breakpoints_.begin(); it != breakpoints_.end();
       ++it) {
    if (*it == address) {
      breakpoints_.erase(it);
      return true;
    }
  }
  return false;
}

bool Debugger::hasBreakpoint(uint32_t address) const {
  for (const uint32_t entry : breakpoints_) {
    if (entry == address) {
      return true;
    }
  }
  return false;
}

const std::vector<uint32_t>& Debugger::breakpoints() const { return breakpoints_; }

void Debugger::addWatchpoint(uint32_t address) {
  if (!hasWatchpoint(address)) {
    watchpoints_.push_back(address);
  }
}

bool Debugger::removeWatchpoint(uint32_t address) {
  for (std::vector<uint32_t>::iterator it = watchpoints_.begin(); it != watchpoints_.end();
       ++it) {
    if (*it == address) {
      watchpoints_.erase(it);
      return true;
    }
  }
  return false;
}

bool Debugger::hasWatchpoint(uint32_t address) const {
  for (const uint32_t entry : watchpoints_) {
    if (entry == address) {
      return true;
    }
  }
  return false;
}

const std::vector<uint32_t>& Debugger::watchpoints() const { return watchpoints_; }

bool Debugger::loadSymbols(const std::string& path, std::string& error) {
  std::ifstream file(path);
  if (!file) {
    error = "cannot open " + path;
    return false;
  }

  std::map<std::string, uint32_t> loaded;
  std::string line;
  std::size_t lineNumber = 0;

  while (std::getline(file, line)) {
    ++lineNumber;

    const std::size_t comment = line.find_first_of("#;");
    if (comment != std::string::npos) {
      line.erase(comment);
    }

    std::istringstream words(line);
    std::string name;
    if (!(words >> name)) {
      continue;
    }

    std::string addressText;
    if (!(words >> addressText)) {
      error = "line " + std::to_string(lineNumber) + " has no address";
      return false;
    }
    if (addressText == "=" && !(words >> addressText)) {
      error = "line " + std::to_string(lineNumber) + " has no address";
      return false;
    }

    uint32_t address = 0;
    if (!parseNumber(addressText, address)) {
      error = "line " + std::to_string(lineNumber) + " has a bad address " + addressText;
      return false;
    }

    loaded[name] = address;
  }

  symbols_ = loaded;
  return true;
}

bool Debugger::findSymbol(const std::string& name, uint32_t& address) const {
  const std::map<std::string, uint32_t>::const_iterator it = symbols_.find(name);
  if (it == symbols_.end()) {
    return false;
  }
  address = it->second;
  return true;
}

std::string Debugger::symbolFor(uint32_t address) const {
  for (const std::pair<const std::string, uint32_t>& entry : symbols_) {
    if (entry.second == address) {
      return entry.first;
    }
  }
  return std::string();
}

void Debugger::reset() {
  cpu_.reset();
  stopReason_ = StopReason::Running;
  stopAddress_ = kCodeBase;
  resumeFromBreakpoint_ = false;
}

}  // namespace risc201
