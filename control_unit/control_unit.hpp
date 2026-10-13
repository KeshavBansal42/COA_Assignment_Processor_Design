#pragma once

// The hardwired decode goes in first, microcode replaces it later.

#include <cstdint>

#include "isa_spec/isa201.h"

namespace risc201 {

// operations for our microcode alu
enum class ALUOperation { 
  ADD, 
  SUB, 
  AND, 
  OR, 
  NONE 
};

// struct to hold the decodded parts of instruction for our microcode
struct DecodedInstruction {
  uint8_t opcode;
  uint8_t rs;
  uint8_t rt;
  uint8_t rd;
  int32_t immediate;
  
  // our microcode control signals
  bool RegWrite;
  bool MemRead;
  bool MemWrite;
  bool Branch;
  bool ALUSrc;
  bool illegal;
  
  ALUOperation aluOp; // newly added field
};

DecodedInstruction decodeInstruction(uint32_t instr);

class ControlUnit {
 public:
  // Turns one instruction word into the signals the pipeline needs.
  ControlSignals decode(uint32_t instr);

  // Which microinstruction the current instruction is on
  uint32_t getMicroPC() const;

 private:
  // keeps track of what rom address we are on
  uint32_t microPc_ = 0; 
};

// alias so keshav cpu and tests dont break
using MicroprogrammedControl = ControlUnit;

}  // namespace risc201