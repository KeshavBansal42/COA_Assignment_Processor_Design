#pragma once

// Overlapped pipelines for the simulator.
//
// Unlike the sequential pipeline (which runs one whole instruction per step),
// this engine advances one clock cycle per call and keeps several instructions
// in flight at once. It supports a 4 stage (IF, ID, EX, WB) and a 6 stage
// (IF, ID, OF, EX, MEM, WB) configuration, with register forwarding, hazard
// stalls and branch flushes. The stalls are conservative: an operand is only
// read once the value it needs is either in the register file or ready to
// forward, so the result is always the same as the sequential reference.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "isa_spec/isa201.h"

namespace risc201 {

class Alu;
class ControlUnit;
class Cpu;
class MemorySafety;

// How the CPU runs a program.
enum class PipelineMode {
  Sequential,  // one whole instruction per step
  FourStage,   // overlapped IF, ID, EX, WB
  SixStage,    // overlapped IF, ID, OF, EX, MEM, WB
};

inline const char* pipelineModeName(PipelineMode mode) {
  switch (mode) {
    case PipelineMode::Sequential:
      return "sequential";
    case PipelineMode::FourStage:
      return "4stage";
    case PipelineMode::SixStage:
      return "6stage";
  }
  return "sequential";
}

// One instruction as it sits in a pipeline latch.
struct PipeLatch {
  bool valid = false;
  uint32_t pc = 0;
  uint32_t instr = 0;
  ControlSignals signals{};

  // operands read at the operand stage
  uint32_t operandA = 0;
  uint32_t operandB = 0;
  uint32_t storeData = 0;  // the register value a store or push will write
  uint32_t sp = 0;

  // results computed along the way
  uint32_t aluResult = 0;
  Flags flags{};
  uint32_t memAddress = 0;
  uint32_t storeValue = 0;
  uint32_t target = 0;
  uint32_t loadValue = 0;
  bool redirect = false;
  bool predictedTaken = false;
  uint32_t predictedNextPc = 0;

  // what this instruction writes, for hazard detection
  bool writesReg = false;
  uint8_t destReg = 0xFF;
  bool writesFlags = false;
  bool writesSp = false;
  uint32_t nextSp = 0;
  bool isLoad = false;

  // true once the value to forward or commit is ready
  bool resultReady = false;
  uint32_t resultValue = 0;

  // set when the instruction halts the machine or raises an exception
  bool stop = false;
  bool stopIsHalt = false;
  ExceptionCode stopException = ExceptionCode::NONE;
};

class CyclePipeline {
 public:
  static constexpr int kMaxStages = 6;

  CyclePipeline(Cpu& cpu, Alu& alu, ControlUnit& control, MemorySafety& safety, int stages);

  // Clears every latch and re-fetches from the CPU's current PC.
  void reset();

  // Runs one clock cycle.
  void cycle();

  int stages() const { return stages_; }
  bool empty() const;
  bool finished() const { return done_; }

  // The address the next fetch will read.
  uint32_t nextFetchPc() const { return fetchPc_; }

  uint64_t cycles() const { return cycles_; }
  uint64_t retired() const { return retired_; }
  uint64_t stalls() const { return stalls_; }
  uint64_t flushes() const { return flushes_; }
  uint64_t forwards() const { return forwards_; }
  uint64_t branchPredictions() const { return branchPredictions_; }
  uint64_t branchMispredictions() const { return branchMispredictions_; }
  void setPredictionEnabled(bool enabled) { predictionEnabled_ = enabled; }

  // Latch 0 is the IF/ID latch, the highest index is the write back input.
  const PipeLatch& latch(int index) const { return lat_[index]; }

  // Names of the stages, oldest last, for the visualizer.
  std::vector<std::string> stageNames() const;

 private:
  int operandStage() const;
  int executeStage() const;
  int memoryStage() const;
  int writeBackStage() const { return stages_ - 1; }

  PipeLatch fetchLatch();
  PipeLatch decodeLatch(const PipeLatch& in);
  PipeLatch operandLatch(const PipeLatch& in, const std::array<PipeLatch, kMaxStages>& ahead);
  PipeLatch executeLatch(const PipeLatch& in);
  PipeLatch memoryLatch(const PipeLatch& in);
  void writeBackLatch(const PipeLatch& in);

  bool detectStall(const PipeLatch& in, const std::array<PipeLatch, kMaxStages>& ahead, int from);
  uint32_t forwardRegister(uint8_t reg, const std::array<PipeLatch, kMaxStages>& ahead, int from);
  uint32_t immediateOf(uint32_t instr) const;

  Cpu& cpu_;
  Alu& alu_;
  ControlUnit& control_;
  MemorySafety& safety_;
  int stages_;

  std::array<PipeLatch, kMaxStages> lat_{};
  uint32_t fetchPc_ = 0;

  // PC-indexed bimodal table, initially weakly not taken.
  static constexpr std::size_t kPredictorEntries = 64;
  std::array<uint8_t, kPredictorEntries> predictor_{};
  bool predictionEnabled_ = true;

  bool draining_ = false;   // a stop was seen, only older instructions drain
  bool done_ = false;       // the machine has halted or faulted
  uint32_t stopPc_ = 0;
  bool stopHalt_ = false;
  ExceptionCode stopException_ = ExceptionCode::NONE;

  uint64_t cycles_ = 0;
  uint64_t retired_ = 0;
  uint64_t stalls_ = 0;
  uint64_t flushes_ = 0;
  uint64_t forwards_ = 0;
  uint64_t branchPredictions_ = 0;
  uint64_t branchMispredictions_ = 0;
};

}  // namespace risc201
