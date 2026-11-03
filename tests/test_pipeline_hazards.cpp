// The overlapped pipelines have to reach exactly the same machine state as the
// sequential reference, hazards and all.

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

using namespace risc201;

namespace {

std::vector<uint32_t> loadProgram(const std::string& name) {
  std::ifstream file(std::string(RISC201_PROGRAMS_DIR) + "/" + name);
  assert(file && "program file missing");

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
      program.push_back(static_cast<uint32_t>(std::stoul(word, nullptr, 16)));
    }
  }
  return program;
}

struct MachineState {
  std::vector<uint32_t> regs;
  std::vector<uint32_t> mem;
  uint32_t pc = 0;
  uint32_t sp = 0;
  Flags flags{};
  bool halted = false;
  ExceptionCode exception = ExceptionCode::NONE;
};

MachineState capture(Cpu& cpu) {
  MachineState state;
  for (std::size_t i = 0; i < kRegisterCount; ++i) {
    state.regs.push_back(cpu.reg(static_cast<uint8_t>(i)));
  }
  for (uint32_t i = 0; i < kMemoryWords; ++i) {
    state.mem.push_back(cpu.readMem(i));
  }
  state.pc = cpu.pc();
  state.sp = cpu.sp();
  state.flags = cpu.flags();
  state.halted = cpu.halted();
  state.exception = cpu.exception();
  return state;
}

bool sameState(const MachineState& a, const MachineState& b) {
  return a.regs == b.regs && a.mem == b.mem && a.pc == b.pc && a.sp == b.sp &&
         a.flags == b.flags && a.halted == b.halted && a.exception == b.exception;
}

MachineState run(const std::vector<uint32_t>& program, PipelineMode mode) {
  Cpu cpu;
  cpu.load(program);
  cpu.setPipelineMode(mode);
  cpu.run();
  return capture(cpu);
}

uint64_t runInstructions(const std::vector<uint32_t>& program, PipelineMode mode) {
  Cpu cpu;
  cpu.load(program);
  cpu.setPipelineMode(mode);
  cpu.run();
  return cpu.instructions();
}

}  // namespace

int main() {
  const char* programs[] = {"loop.hex",         "recursion.hex",       "stack_fault.hex",
                            "stack_macros.hex", "hazard_raw.hex",      "hazard_load_use.hex",
                            "hazard_branch.hex"};

  int mismatches = 0;
  for (const char* name : programs) {
    const std::vector<uint32_t> program = loadProgram(name);
    const MachineState sequential = run(program, PipelineMode::Sequential);

    for (PipelineMode mode : {PipelineMode::FourStage, PipelineMode::SixStage}) {
      const MachineState overlapped = run(program, mode);
      const bool equal = sameState(sequential, overlapped);
      std::cout << name << " " << pipelineModeName(mode) << (equal ? " matches" : " DIFFERS")
                << "\n";
      if (!equal) {
        ++mismatches;
      }
    }
  }

  // The retired instruction total has to agree across all three executors.
  for (const char* name : programs) {
    const std::vector<uint32_t> program = loadProgram(name);
    const uint64_t sequential = runInstructions(program, PipelineMode::Sequential);
    for (PipelineMode mode : {PipelineMode::FourStage, PipelineMode::SixStage}) {
      const uint64_t overlapped = runInstructions(program, mode);
      std::cout << name << " " << pipelineModeName(mode) << " instructions "
                << overlapped << " vs sequential " << sequential << "\n";
      if (overlapped != sequential) {
        ++mismatches;
      }
    }
  }

  // The overlapped pipelines really do overlap, so they take more cycles than
  // the instructions they retire.
  Cpu cpu;
  cpu.load(loadProgram("loop.hex"));
  cpu.setPipelineMode(PipelineMode::SixStage);
  cpu.run();
  const bool overlapped = cpu.cycles() > cpu.instructions();
  std::cout << "6stage cycles=" << cpu.cycles() << " instructions=" << cpu.instructions()
            << (overlapped ? " (overlapped)\n" : " (unexpected)\n");
  if (!overlapped) {
    ++mismatches;
  }

  std::cout << "mismatches=" << mismatches << "\n";
  assert(mismatches == 0);
  return 0;
}
