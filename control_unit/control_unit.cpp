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

} // namespace risc201

// hardwired previous logic


// namespace risc201 {

// namespace {

// // proper risc201 shifts based on our isa201.h specs
// static inline uint8_t extractOpcode(uint32_t instr) {
//   return static_cast<uint8_t>((instr >> 26) & 0x3F);
// }

// static inline uint8_t extractRd(uint32_t instr) {
//   return static_cast<uint8_t>((instr >> 22) & 0x0F);
// }

// static inline uint8_t extractRs(uint32_t instr) {
//   return static_cast<uint8_t>((instr >> 18) & 0x0F);
// }

// static inline uint8_t extractRt(uint32_t instr) {
//   return static_cast<uint8_t>((instr >> 14) & 0x0F);
// }

// // 18-bit sign extension for risc201 immediates
// static inline int32_t signExtend(uint32_t imm) {
//   // if 18th bit (index 17) is 1, its a negative number
//   if (imm & 0x20000) {
//     return static_cast<int32_t>(imm | 0xFFFC0000);
//   }
//   return static_cast<int32_t>(imm);
// }

// static inline int32_t extractImmediate(uint32_t instr) {
//   return signExtend(instr & 0x3FFFF);
// }

// } // namespace

// // crack open the instruction and fill our struct
// DecodedInstruction decodeInstruction(uint32_t instr) {
//   DecodedInstruction dec;
  
//   // initialize everything to safe default values
//   dec.opcode = 0;
//   dec.rs = 0;
//   dec.rt = 0;
//   dec.rd = 0;
//   dec.immediate = 0;
//   dec.RegWrite = false;
//   dec.MemRead = false;
//   dec.MemWrite = false;
//   dec.Branch = false;
//   dec.ALUSrc = false;
//   dec.aluOp = ALUOperation::NONE;
  
//   // extract the actual bits (this already handles rs, rt, rd, and imm!)
//   dec.opcode = extractOpcode(instr);
//   dec.rd = extractRd(instr);
//   dec.rs = extractRs(instr);
//   dec.rt = extractRt(instr);
//   dec.immediate = extractImmediate(instr);
  
//   // set specific signals depending on what instruction it is (using RISC201 opcodes)
//   switch (static_cast<Opcode>(dec.opcode)) {
//     // R-Type logic (risc201 doesnt use a funct field, ADD and SUB have their own opcodes)
//     case Opcode::ADD:
//       dec.RegWrite = true;
//       dec.ALUSrc = false;
//       dec.aluOp = ALUOperation::ADD;
//       break;
      
//     case Opcode::SUB:
//       dec.RegWrite = true;
//       dec.ALUSrc = false;
//       dec.aluOp = ALUOperation::SUB;
//       break;
      
//     // Load Word
//     case Opcode::LW:
//       dec.RegWrite = true;
//       dec.MemRead = true;
//       dec.ALUSrc = true;
//       dec.aluOp = ALUOperation::ADD; // we need add to calculate memory address
//       break;
      
//     // Store Word
//     case Opcode::SW:
//       dec.MemWrite = true;
//       dec.ALUSrc = true;
//       dec.aluOp = ALUOperation::ADD; // we need add to calculate memory address
//       break;
      
//     // Branch if Equal
//     case Opcode::BEQ:
//       dec.Branch = true;
//       dec.ALUSrc = false;
//       dec.RegWrite = false;
//       dec.aluOp = ALUOperation::SUB; // use sub to check if they are equal
//       break;
      
//     default:
//         dec.illegal=true;
//       break;
//   }
  
//   return dec;
// }

// // the real deal: horizontal microcoded control unit!
// class MicroprogrammedControl : public ControlUnit {
//  public:
//   ControlSignals decode(uint32_t instr) override;
//   uint32_t getMicroPC() const override;

//  private:
//   // keeps track of what rom address we are on
//   uint32_t microPc_ = 0; 
// };

// } // namespace risc201