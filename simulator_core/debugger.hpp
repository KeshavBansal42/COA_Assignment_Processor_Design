#pragma once

// Breakpoints, watchpoints and the symbol table behind the debugger commands.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "simulator_core/cpu.hpp"

namespace risc201 {

enum class StopReason {
  Running,
  Breakpoint,
  Watchpoint,
  Halted,
  Exception,
};

class Debugger {
 public:
  explicit Debugger(Cpu& cpu);

  // Runs one instruction and reports why the machine stopped.
  StopReason stepOnce();

  // Runs until a breakpoint, a watchpoint, HALT or an exception.
  StopReason continueRun();

  StopReason stopReason() const;
  uint32_t stopAddress() const;

  void addBreakpoint(uint32_t address);
  bool removeBreakpoint(uint32_t address);
  bool hasBreakpoint(uint32_t address) const;
  const std::vector<uint32_t>& breakpoints() const;

  void addWatchpoint(uint32_t address);
  bool removeWatchpoint(uint32_t address);
  bool hasWatchpoint(uint32_t address) const;
  const std::vector<uint32_t>& watchpoints() const;

  // Symbol lines are "name address" or "name = address".
  bool loadSymbols(const std::string& path, std::string& error);
  bool findSymbol(const std::string& name, uint32_t& address) const;

  // Name for an address, an empty string when there is none.
  std::string symbolFor(uint32_t address) const;

  // Resets the CPU, breakpoints, watchpoints and symbols stay put.
  void reset();

 private:
  StopReason stop(StopReason reason, uint32_t address);

  Cpu& cpu_;
  std::vector<uint32_t> breakpoints_;
  std::vector<uint32_t> watchpoints_;
  std::map<std::string, uint32_t> symbols_;
  StopReason stopReason_ = StopReason::Running;
  uint32_t stopAddress_ = kCodeBase;
  bool resumeFromBreakpoint_ = false;
};

}  // namespace risc201
