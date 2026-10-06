#include "control_unit/control_unit.hpp"
#include "control_unit/control_rom.hpp"

namespace risc201 {

namespace {

// proper risc201 shift to get the opcode for our ROM address
static inline uint8_t extractOpcode(uint32_t instr) {
  return static_cast<uint8_t>((instr >> 26) & 0x3F);
}

} // namespace

// decode by reading from the ROM instead of a switch block!
ControlSignals MicroprogrammedControl::decode(uint32_t instr) {
  ControlSignals signals;
  
  // extract opcode to use as the ROM memory address
  uint8_t op = extractOpcode(instr);
  signals.opcode = static_cast<Opcode>(op);
  
  // fetch the 32-bit microinstruction from our simulated ROM chip
  uint32_t microword = CONTROL_ROM[op];
  
  // update micro pc for the debugger (we are horizontal so its just 1 step)
  microPc_ = op;
  
  // unpack the physical wires back into Keshavs pipeline struct
  using namespace MicrocodeBits;
  signals.regWrite   = (microword & REG_WRITE) != 0;
  signals.memRead    = (microword & MEM_READ) != 0;
  signals.memWrite   = (microword & MEM_WRITE) != 0;
  signals.branch     = (microword & BRANCH) != 0;
  signals.jump       = (microword & JUMP) != 0;
  signals.push       = (microword & PUSH) != 0;
  signals.pop        = (microword & POP) != 0;
  signals.ret        = (microword & RET) != 0;
  signals.halt       = (microword & HALT) != 0;
  signals.writeFlags = (microword & WRITE_FLAGS) != 0;
  signals.illegal    = (microword & ILLEGAL) != 0;
  
  // mux selectors
  signals.aluSrc     = (microword & ALU_SRC_IMM) ? AluSrc::Immediate : AluSrc::Register;
  signals.resultSrc  = (microword & RES_SRC_MEM) ? ResultSrc::Memory : ResultSrc::Alu;
  
  // extract the 4-bit ALU operation enum
  uint32_t aluOpInt  = (microword >> ALU_OP_SHIFT) & ALU_OP_MASK;
  signals.aluOp      = static_cast<AluOp>(aluOpInt);
  
  return signals;
}

uint32_t MicroprogrammedControl::getMicroPC() const {
  return microPc_;
}

}
