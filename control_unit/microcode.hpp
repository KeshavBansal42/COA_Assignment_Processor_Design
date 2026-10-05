#pragma once

#include <cstdint>

namespace risc201 {

// defining the exact hardware wires for vikrams horizontal microcode!
// every bit in our 32-bit ROM word is a physical control wire.

namespace MicrocodeBits {

  // single bit control flags (bits 0 to 10)
  inline constexpr uint32_t REG_WRITE   = 1 << 0;
  inline constexpr uint32_t MEM_READ    = 1 << 1;
  inline constexpr uint32_t MEM_WRITE   = 1 << 2;
  inline constexpr uint32_t BRANCH      = 1 << 3;
  inline constexpr uint32_t JUMP        = 1 << 4;
  inline constexpr uint32_t PUSH        = 1 << 5;
  inline constexpr uint32_t POP         = 1 << 6;
  inline constexpr uint32_t RET         = 1 << 7;
  inline constexpr uint32_t HALT        = 1 << 8;
  inline constexpr uint32_t WRITE_FLAGS = 1 << 9;
  inline constexpr uint32_t ILLEGAL     = 1 << 10;
  
  // mux selectors (1 bit each)
  inline constexpr uint32_t ALU_SRC_IMM = 1 << 11; // 1 = Immediate, 0 = Register
  inline constexpr uint32_t RES_SRC_MEM = 1 << 12; // 1 = Memory, 0 = ALU
  
  // ALU Operation (we need 4 bits for this, so bits 13 to 16)
  inline constexpr uint32_t ALU_OP_SHIFT = 13;
  inline constexpr uint32_t ALU_OP_MASK  = 0x0F; 

} // namespace MicrocodeBits

} // namespace risc201