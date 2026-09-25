#pragma once

// Interactive command shell for the simulator.

#include <iosfwd>
#include <string>

#include "simulator_core/cpu.hpp"

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
  void commandRegs() const;
  void commandMem(uint32_t start, uint32_t count) const;
  void printStatus() const;
  void printHelp() const;

  Cpu& cpu_;
  std::ostream& out_;
};

}  // namespace risc201
