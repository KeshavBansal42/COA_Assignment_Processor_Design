#pragma once

// Simulator core: the machine state and the run loop.
// The CLI and the debugger only touch the CPU through this class.

#include <array>
#include <cstdint>
#include <vector>

#include "control_unit/control_unit.hpp"
#include "alu/alu.hpp"
#include "simulator_core/cycle_pipeline.hpp"
#include "simulator_core/pipeline.hpp"
#include "memory_safety/memory_safety.hpp"

namespace risc201 {

class Cpu {
public:
  Cpu();

  // Clears memory, copies the program into the code region and resets the
  // state.
  void load(const std::vector<uint32_t> &program);

  // Runs one instruction.
  void step();

  // Runs until HALT, an exception or the end of the code region.
  void run();

  // Puts the registers, PC, SP, FLAGS, the halt flag and the exception back to
  // their start values. Memory is kept so the loaded program survives a reset.
  void reset();

  // All the getter-setter functions.

  uint32_t reg(uint8_t index) const;
  void setReg(uint8_t index, uint32_t value);

  uint32_t pc() const;
  void setPc(uint32_t value);

  uint32_t sp() const;
  void setSp(uint32_t value);

  Flags flags() const;
  void setFlags(const Flags &value);

  // Word addressed memory, one 32-bit word per address.
  uint32_t readMem(uint32_t address) const;
  void writeMem(uint32_t address, uint32_t value);

  // get raw memory for stack visualizer
  const uint32_t *rawMemory() const {
    return memory_.data();
  } // added afer memory_safety part

  bool halted() const;
  void setHalted(bool value);

  // NONE until an instruction raises an exception, which stops the machine.
  ExceptionCode exception() const;
  void setException(ExceptionCode value);

  // True when the last step wrote memory, with the address it wrote to.
  bool wroteMemory() const;
  uint32_t lastWriteAddress() const;

  // Which pipeline runs the program. Sequential is the default.
  void setPipelineMode(PipelineMode mode);
  PipelineMode pipelineMode() const;

  // Which control memory the control unit reads. Horizontal is the default.
  void setControlMode(ControlMode mode);
  ControlMode controlMode() const;

  // Decode one instruction word without running it, for the CLI's microcode
  // inspector. Refreshes the control unit's microPC.
  ControlSignals decodeInstruction(uint32_t instr);

  // The control memory words an instruction walks, for the microPC trace.
  std::vector<MicroTraceEntry> microTrace(uint32_t instr) const;

  // Address the active pipeline will fetch next. Equal to pc() in sequential
  // mode; in the overlapped modes it is the fetch stage's PC, which is ahead of
  // the last committed write back.
  uint32_t nextPc() const;

  // Clock cycles and retired instructions since the last reset.
  uint64_t cycles() const;
  uint64_t instructions() const;

  // Hazard counters, zero in sequential mode.
  uint64_t stalls() const;
  uint64_t flushes() const;
  uint64_t forwards() const;
  uint64_t branchPredictions() const;
  uint64_t branchMispredictions() const;
  void setBranchPrediction(bool enabled);
  bool branchPredictionEnabled() const { return branchPredictionEnabled_; }

  // The overlapped pipeline in use, for the visualizer and the evaluation run.
  const CyclePipeline& cyclePipeline() const;

  // Memory safety state, so the CLI and the visualizer can show the stack
  // floor and the heap/stack split.
  const MemorySafetyImpl& memorySafety() const { return safety_; }

  // Switches the stack between the fixed 0xC00 floor and a floor that follows
  // the heap high-water mark, letting the stack grow into the data region.
  void setDynamicStack(bool enable) { safety_.enableDynamicStack(enable); }
  bool dynamicStack() const { return safety_.isDynamicStackEnabled(); }

  // The entire state and memory of our CPU.
private:
  std::array<uint32_t, kRegisterCount> registers_{};
  std::array<uint32_t, kMemoryWords> memory_{};
  uint32_t pc_ = kCodeBase;
  uint32_t sp_ = kInitialSp;
  Flags flags_{};
  bool halted_ = false;
  ExceptionCode exception_ = ExceptionCode::NONE;
  bool wroteMemory_ = false;
  uint32_t lastWriteAddress_ = 0;

  MicroprogrammedControl control_{};
  ConcreteAlu alu_{};
  MemorySafetyImpl safety_{};

  Pipeline pipeline_;
  CyclePipeline cycle4_{*this, alu_, control_, safety_, 4};
  CyclePipeline cycle6_{*this, alu_, control_, safety_, 6};

  PipelineMode mode_ = PipelineMode::Sequential;
  bool branchPredictionEnabled_ = true;
  uint64_t cycles_ = 0;
  uint64_t instructions_ = 0;
};

} // namespace risc201
