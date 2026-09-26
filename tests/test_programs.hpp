#pragma once

#include <vector>
#include <cstdint>
#include "isa_spec/isa201.h"

namespace risc201 {

// simple loop program that countdowns from 5
inline const std::vector<uint32_t> kLoopProgram = {
    encodeI(Opcode::LDI, 1, 0, 5),     // 0: LDI r1, 5
    encodeI(Opcode::ADDI, 1, 1, -1),   // 1: loop: ADDI r1, r1, -1 (this updates flags)
    encodeJ(Opcode::BNE, -2),          // 2: BNE loop (target = PC(2) + 1 - 2 = 1)
    encodeOpcode(Opcode::HALT)         // 3: HALT
};

// simple recursive func that calls itself 3 times
inline const std::vector<uint32_t> kRecursionProgram = {
    encodeI(Opcode::LDI, 1, 0, 3),     // 0: main: LDI r1, 3
    encodeJ(Opcode::CALL, 1),          // 1: CALL func (target = PC(1) + 1 + 1 = 3)
    encodeOpcode(Opcode::HALT),        // 2: HALT
                                       // func:
    encodeI(Opcode::ADDI, 1, 1, -1),   // 3: ADDI r1, r1, -1
    encodeJ(Opcode::BEQ, 1),           // 4: BEQ end (target = PC(4) + 1 + 1 = 6)
    encodeJ(Opcode::CALL, -3),         // 5: CALL func (target = PC(5) + 1 - 3 = 3)
                                       // end:
    encodeI(Opcode::RET, 0, 0, 0)      // 6: RET
};

// infinite push loop to trigger stack overflow exception
inline const std::vector<uint32_t> kStackOverflowProgram = {
    encodeI(Opcode::PUSH, 1, 0, 0),    // 0: loop: PUSH r1
    encodeJ(Opcode::JMP, -2)           // 1: JMP loop (target = PC(1) + 1 - 2 = 0)
};

} // namespace risc201