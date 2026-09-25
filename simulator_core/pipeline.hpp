#pragma once

// Four stage pipeline: IF, ID/OF, EX, WB.
// One instruction moves through all four stages per step, so the stages do not
// overlap yet and hazards are not modelled.

#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

class Alu;
class ControlUnit;
class Cpu;
class MemorySafety;

class Pipeline {
 public:
  Pipeline(Cpu& cpu, Alu& alu, ControlUnit& control, MemorySafety& safety);

  // Runs one instruction through the four stages.
  void step();

  // Clears the pipeline latches.
  void reset();

 private:
  // IF/ID latch, the fetched instruction and the address it came from.
  struct IfId {
    uint32_t instruction = 0;
    uint32_t pc = 0;
  };

  // ID/EX latch, the decoded signals and the register operands.
  struct IdEx {
    ControlSignals signals{};
    uint32_t instruction = 0;
    uint32_t operandA = 0;
    uint32_t operandB = 0;
    uint32_t pc = 0;
  };

  // EX/WB latch, the ALU result and everything the write back stage needs.
  struct ExWb {
    ControlSignals signals{};
    uint32_t instruction = 0;
    uint32_t pc = 0;
    uint32_t result = 0;
    uint32_t storeValue = 0;
    uint32_t memoryAddress = 0;
    uint32_t nextSp = 0;
    uint32_t target = 0;
    Flags flags{};
    bool updateSp = false;
    bool redirect = false;
  };

  void fetch();
  void decode();
  void execute();
  void writeBack();

  // True once the CPU has an exception, which stops the remaining stages.
  bool stopped() const;

  Cpu& cpu_;
  Alu& alu_;
  ControlUnit& control_;
  MemorySafety& safety_;

  IfId ifId_{};
  IdEx idEx_{};
  ExWb exWb_{};
};

}  // namespace risc201
