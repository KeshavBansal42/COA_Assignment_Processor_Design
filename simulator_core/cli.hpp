#pragma once

// Interactive command shell and debugger for the simulator.

#include <iosfwd>
#include <string>

#include "simulator_core/cpu.hpp"
#include "simulator_core/debugger.hpp"

namespace risc201 {

class Cli {
 public:
  Cli(Cpu& cpu, std::ostream& out);

  // Runs one command line, returns false when the session should end.
  bool execute(const std::string& line);

 private:
  void commandLoad(const std::string& path);
  void commandRun();
  void commandStep(int count);
  void commandReset();
  void commandRegs() const;
  void commandMem(uint32_t start, uint32_t count) const;

  void commandStack(int maxDepth) const; // added after memory_safety
  void commandDynamic(const std::string& state);

  void commandMode(const std::string& name);
  void commandMicro(const std::string& name);
  void commandMicroTrace(uint32_t address);
  void commandCycles() const;

  void commandBreak(const std::string& target);
  void commandDelete(const std::string& target);
  void commandBreaks() const;
  void commandWatch(const std::string& target);
  void commandUnwatch(const std::string& target);
  void commandWatches() const;
  void commandSymbols(const std::string& path);

  void printStop() const;
  void printStatus() const;
  void printHelp() const;

  // Accepts a number or a symbol name.
  bool resolveAddress(const std::string& text, uint32_t& address, std::string& error) const;
  std::string describe(uint32_t address) const;

  Cpu& cpu_;
  Debugger debugger_;
  std::ostream& out_;
};

}  // namespace risc201
