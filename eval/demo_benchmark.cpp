// Presentation benchmark: bounded runs and architectural-state checks.
#include <array>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "simulator_core/cpu.hpp"

using namespace risc201;

struct Case {
  const char* name;
  const char* file;
  uint8_t reg;
  uint32_t value;
  bool checkMemory;
};

const Case cases[] = {
    {"Loop", "loop.hex", 3, 55, false},
    {"Long independent stream", "bench_independent.hex", 15, 16, false},
    {"Long dependent chain", "bench_dependent.hex", 1, 192, false},
    {"Independent writes", "demo_independent.hex", 7, 77, false},
    {"Register forwarding", "hazard_raw.hex", 6, 493, false},
    {"Load-use dependency", "hazard_load_use.hex", 4, 200, false},
    {"Taken / not-taken branch", "demo_branch_paths.hex", 4, 42, false},
    {"HALT and older writes", "demo_halt_drain.hex", 3, 17, true},
};

std::vector<uint32_t> load(const char* file) {
  std::ifstream input(std::string(RISC201_PROGRAMS_DIR) + "/" + file);
  if (!input) throw std::runtime_error(std::string("cannot open ") + file);
  std::vector<uint32_t> words;
  std::string line, token;
  while (std::getline(input, line)) {
    line = line.substr(0, line.find_first_of("#;"));
    std::istringstream in(line);
    while (in >> token) words.push_back(static_cast<uint32_t>(std::stoul(token, nullptr, 16)));
  }
  return words;
}

struct Result {
  std::array<uint32_t, kRegisterCount> regs{};
  std::array<uint32_t, kMemoryWords> memory{};
  uint32_t pc = 0, sp = 0;
  Flags flags{};
  uint64_t cycles = 0, instructions = 0, stalls = 0, forwards = 0, flushes = 0;
  uint64_t branches = 0, misses = 0;
  unsigned peakInFlight = 0;
  bool halted = false;
  ExceptionCode exception = ExceptionCode::NONE;
};

Result run(const std::vector<uint32_t>& program, PipelineMode mode, bool predict = true) {
  Cpu cpu;
  cpu.setBranchPrediction(predict);
  cpu.load(program);
  cpu.setPipelineMode(mode);
  constexpr uint64_t limit = 10000;
  unsigned peak = 0;
  while (!cpu.halted() && cpu.exception() == ExceptionCode::NONE && cpu.cycles() < limit) {
    cpu.step();
    unsigned active = 0;
    if (mode == PipelineMode::Sequential) {
      active = 1;
    } else {
      const auto& pipeline = cpu.cyclePipeline();
      for (int i = 0; i < pipeline.stages(); ++i) active += pipeline.latch(i).valid ? 1u : 0u;
    }
    if (active > peak) peak = active;
  }
  Result result;
  result.peakInFlight = peak;
  for (std::size_t i = 0; i < result.regs.size(); ++i) result.regs[i] = cpu.reg(static_cast<uint8_t>(i));
  for (std::size_t i = 0; i < result.memory.size(); ++i) result.memory[i] = cpu.readMem(static_cast<uint32_t>(i));
  result.pc = cpu.pc(); result.sp = cpu.sp(); result.flags = cpu.flags();
  result.cycles = cpu.cycles(); result.instructions = cpu.instructions();
  result.stalls = cpu.stalls(); result.forwards = cpu.forwards(); result.flushes = cpu.flushes();
  result.branches = cpu.branchPredictions(); result.misses = cpu.branchMispredictions();
  result.halted = cpu.halted(); result.exception = cpu.exception();
  return result;
}

// Time execution only, excluding loading, reset, state comparison and output.
// Repetition reduces clock quantization for these very short demo programs.
double meanExecutionUs(const std::vector<uint32_t>& program, PipelineMode mode) {
  using Clock = std::chrono::steady_clock;
  constexpr int warmups = 5;
  constexpr int samples = 100;
  constexpr uint64_t limit = 10000;
  Cpu cpu;
  cpu.setPipelineMode(mode);
  Clock::duration elapsed{};
  for (int i = 0; i < warmups + samples; ++i) {
    cpu.load(program);
    const auto start = Clock::now();
    while (!cpu.halted() && cpu.exception() == ExceptionCode::NONE && cpu.cycles() < limit) cpu.step();
    const auto end = Clock::now();
    if (!cpu.halted() || cpu.exception() != ExceptionCode::NONE) {
      throw std::runtime_error("timed run did not halt successfully");
    }
    if (i >= warmups) elapsed += end - start;
  }
  return std::chrono::duration<double, std::micro>(elapsed).count() / samples;
}

bool matches(const Result& a, const Result& b) {
  return a.regs == b.regs && a.memory == b.memory && a.pc == b.pc && a.sp == b.sp &&
         a.flags == b.flags && a.halted == b.halted && a.exception == b.exception &&
         a.instructions == b.instructions;
}

// Compare policies on identical program state, with a fresh cold predictor per run.
int branchBenchmark() {
  const auto program = load("loop.hex");
  const Result reference = run(program, PipelineMode::Sequential);
  bool allGood = reference.halted && reference.exception == ExceptionCode::NONE &&
                 reference.regs[3] == 55;
  std::cout << "BRANCH PREDICTION BENCHMARK: loop.hex\n"
            << "Sum 10 down to 1: R3 = 55. The branch runs 10 times: 9 taken, 1 not taken.\n"
            << "Static always predicts not taken; dynamic uses a cold 2-bit predictor.\n"
            << "Cycles are simulated; PASS checks final state against the sequential reference.\n\n"
            << "  pipeline  policy                 branches  misses  correct  accuracy  cycles  retired  R3  state\n";
  for (PipelineMode mode : {PipelineMode::FourStage, PipelineMode::SixStage}) {
    for (bool predict : {false, true}) {
      const Result result = run(program, mode, predict);
      const bool pass = reference.halted && reference.exception == ExceptionCode::NONE &&
                        reference.regs[3] == 55 && result.halted && matches(reference, result);
      allGood = allGood && pass;
      const uint64_t correct = result.branches - result.misses;
      const double accuracy = result.branches ? 100.0 * correct / result.branches : 0.0;
      std::cout << "  " << std::left << std::setw(10) << pipelineModeName(mode)
                << std::setw(23) << (predict ? "2-bit dynamic" : "static not-taken")
                << std::right << std::setw(8) << result.branches << std::setw(8) << result.misses
                << std::setw(9) << correct << std::setw(9) << std::fixed << std::setprecision(1)
                << accuracy << "%" << std::setw(8) << result.cycles << std::setw(9)
                << result.instructions << std::setw(4) << result.regs[3] << "  "
                << (pass ? "PASS" : "FAIL") << '\n';
    }
  }
  std::cout << "\nEach policy starts from the same program state; predictor history resets on load.\n"
               "Accuracy counts resolved conditional branch directions, not hardware execution time.\n";
  return allGood ? 0 : 1;
}

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--branch-prediction") return branchBenchmark();
    if (argc != 1) {
      std::cerr << "usage: risc201-demo-benchmark [--branch-prediction]\n";
      return 2;
    }
    bool allGood = true;
    std::cout << "RISC201 PIPELINE DEMO BENCHMARK\n"
              << "Cycles are simulated. Host time is mean execution time over 100 runs (5 warmups),\n"
              << "excluding load/reset. It measures this simulator, not processor clock speed.\n"
              << "Sequential is modeled as one simulated cycle per instruction.\n"
              << "Conditional branches use a 2-bit predictor; counts are resolved branches and direction misses.\n\n";
    for (const Case& example : cases) {
      const auto program = load(example.file);
      const Result reference = run(program, PipelineMode::Sequential);
      bool good = reference.halted && reference.exception == ExceptionCode::NONE &&
                  reference.regs[example.reg] == example.value &&
                  (!example.checkMemory || (reference.regs[2] == 42 && reference.memory[kDataBase] == 42));
      if (std::string(example.file) == "demo_branch_paths.hex") good = good && reference.regs[3] == 7;
      std::cout << example.name << "  (" << example.file << ")\n";
      std::cout << "  mode          cycles  vs seq  retired   CPI   IPC  peak*  stalls  forwards  flush-events  branches  misses   host us/run  state\n";
      for (PipelineMode mode : {PipelineMode::Sequential, PipelineMode::FourStage, PipelineMode::SixStage}) {
        const Result current = mode == PipelineMode::Sequential ? reference : run(program, mode);
        bool pass = good && current.halted && matches(reference, current);
        if (std::string(example.file) == "bench_independent.hex") {
          if (mode == PipelineMode::FourStage) pass = pass && current.peakInFlight >= 2;
          if (mode == PipelineMode::SixStage) pass = pass && current.peakInFlight >= 4;
        }
        const double hostUs = meanExecutionUs(program, mode);
        allGood = allGood && pass;
        std::cout << "  " << std::left << std::setw(14) << pipelineModeName(mode) << std::right
                  << std::setw(6) << current.cycles
                  << std::setw(8) << (static_cast<int64_t>(current.cycles) - static_cast<int64_t>(reference.cycles))
                  << std::setw(9) << current.instructions
                  << std::fixed << std::setprecision(2) << std::setw(7)
                  << (current.instructions ? static_cast<double>(current.cycles) / current.instructions : 0.0)
                  << std::setw(6) << (current.cycles ? static_cast<double>(current.instructions) / current.cycles : 0.0)
                  << std::setw(7) << current.peakInFlight << std::setw(8) << current.stalls << std::setw(10) << current.forwards
                  << std::setw(14) << current.flushes << std::setw(10) << current.branches
                  << std::setw(8) << current.misses << std::fixed << std::setprecision(2)
                  << std::setw(14) << hostUs << "  " << (pass ? "PASS" : "FAIL") << '\n';
      }
      std::cout << '\n';
    }
    std::cout << "vs seq = extra simulated cycles relative to sequential; CPI = cycles / retired instructions.\n"
                 "IPC = retired instructions / simulated cycle.\n"
                 "* Peak is the maximum valid in-flight latches after a cycle (sequential shown as 1).\n"
                 "PASS compares registers, memory, PC, SP, flags, halt and retired count against sequential,\n"
                 "and checks each example's expected result. Flush events also include unconditional redirects;\n"
                 "branch misses count conditional direction predictions, separately from flushes.\n";
    return allGood ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Benchmark error: " << error.what() << '\n';
    return 1;
  }
}
