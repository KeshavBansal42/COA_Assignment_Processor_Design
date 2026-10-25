// Evaluation harness: run every program under each pipeline and each control
// encoding and report the numbers. Prints a markdown table and, when given a
// path, writes it there.

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "control_unit/vertical_microcode.hpp"
#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

using namespace risc201;

namespace {

const char* kPrograms[] = {"loop.hex",          "recursion.hex",      "stack_fault.hex",
                           "hazard_raw.hex",    "hazard_load_use.hex", "hazard_branch.hex"};

std::vector<uint32_t> loadProgram(const std::string& name) {
  std::ifstream file(std::string(RISC201_PROGRAMS_DIR) + "/" + name);
  if (!file) {
    std::cerr << "cannot open " << name << "\n";
    std::exit(1);
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
      program.push_back(static_cast<uint32_t>(std::stoul(word, nullptr, 16)));
    }
  }
  return program;
}

// Control words the run reads: one wide word per instruction for horizontal,
// the microprogram length for vertical.
uint64_t controlWords(const std::vector<uint32_t>& program, bool vertical) {
  uint64_t total = 0;
  for (const uint32_t word : program) {
    const Opcode opcode = decodeOpcode(word);
    if (!isDefinedOpcode(opcode)) {
      ++total;
      continue;
    }
    total += vertical ? microProgramLength(opcode) : 1;
  }
  return total;
}

struct Result {
  std::string program;
  std::string pipeline;
  std::string microcode;
  uint64_t instructions = 0;
  uint64_t cycles = 0;
  uint64_t stalls = 0;
  uint64_t flushes = 0;
  uint64_t forwards = 0;
  uint64_t controlWords = 0;
  bool halted = false;
  ExceptionCode exception = ExceptionCode::NONE;
};

Result runOne(const std::string& name, const std::vector<uint32_t>& program,
              PipelineMode pipeline, ControlMode control) {
  Cpu cpu;
  cpu.load(program);
  cpu.setControlMode(control);
  cpu.setPipelineMode(pipeline);
  cpu.run();

  Result result;
  result.program = name;
  result.pipeline = pipelineModeName(pipeline);
  result.microcode = control == ControlMode::Vertical ? "vertical" : "horizontal";
  result.instructions = cpu.instructions();
  result.cycles = cpu.cycles();
  result.stalls = cpu.stalls();
  result.flushes = cpu.flushes();
  result.forwards = cpu.forwards();
  result.controlWords = controlWords(program, control == ControlMode::Vertical);
  result.halted = cpu.halted();
  result.exception = cpu.exception();
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<Result> results;

  for (const char* name : kPrograms) {
    const std::vector<uint32_t> program = loadProgram(name);
    for (PipelineMode pipeline : {PipelineMode::Sequential, PipelineMode::FourStage,
                                  PipelineMode::SixStage}) {
      for (ControlMode control : {ControlMode::Horizontal, ControlMode::Vertical}) {
        results.push_back(runOne(name, program, pipeline, control));
      }
    }
  }

  std::ostringstream md;
  md << "# Pipeline and microcode evaluation\n\n";
  md << "Runs every demo and hazard program under each pipeline and each control\n";
  md << "encoding. Cycles and instructions are per run, CPI is cycles per retired\n";
  md << "instruction, and the control columns are how many control memory words\n";
  md << "and bits the run reads (one 32-bit word per instruction for horizontal,\n";
  md << "the microprogram length in 16-bit words for vertical).\n\n";
  md << "| Program | Pipeline | Microcode | Instructions | Cycles | CPI | Stalls | "
        "Flushes | Forwards | Control words | Control bits |\n";
  md << "|---|---|---|---|---|---|---|---|---|---|---|\n";

  for (const Result& r : results) {
    const double cpi = r.instructions ? static_cast<double>(r.cycles) / r.instructions : 0.0;
    const uint64_t width = r.microcode == "vertical" ? 16 : 32;
    md << "| " << r.program << " | " << r.pipeline << " | " << r.microcode << " | "
       << r.instructions << " | " << r.cycles << " | " << std::fixed << std::setprecision(2)
       << cpi << " | " << r.stalls << " | " << r.flushes << " | " << r.forwards << " | "
       << r.controlWords << " | " << r.controlWords * width << " |\n";
  }
  md << "\n";

  std::cout << md.str();

  if (argc > 1) {
    std::ofstream out(argv[1]);
    if (!out) {
      std::cerr << "cannot write " << argv[1] << "\n";
      return 1;
    }
    out << md.str();
  }
  return 0;
}
