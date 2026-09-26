# RISC201 ISA specification (frozen 25 Sept)

This file and `isa_spec/isa201.h` are the frozen contract, all modules build against them.
Changes after the freeze need a team sync.

Words are 32-bit and memory is word addressed.

## Registers

- R0-R15: 16 general purpose 32-bit registers. There is no hardwired zero register.
- PC (32-bit): index of the next instruction, starts at 0x000.
- SP (32-bit): full descending stack pointer, decrement first then store, so it points at the
  last valid item. Starts at 0x1000.
- FLAGS: Z, N, C, V.

## Encoding

The opcode is always bits 31-26.

| Type | Layout |
|---|---|
| R | op[6] \| rd[4] \| rs[4] \| rt[4] \| unused[14] |
| I | op[6] \| rd[4] \| rs[4] \| imm[18] signed |
| J | op[6] \| offset[26] signed, relative to PC+1 |

Field positions: op 31-26, rd 25-22, rs 21-18, rt 17-14, imm 17-0, offset 25-0.

## Instruction set (27 instructions)

| Op (hex) | Mnemonic | Type | Semantics |
|---|---|---|---|
| 00 | NOP | - | No operation |
| 01 | HALT | - | Stop execution |
| 02 | ADD rd, rs, rt | R | rd = rs + rt, sets Z N C V |
| 03 | SUB rd, rs, rt | R | rd = rs - rt, sets Z N C V |
| 04 | MUL rd, rs, rt | R | rd = rs * rt (low 32 bits), sets Z N C V |
| 05 | DIV rd, rs, rt | R | rd = rs / rt, DIV_BY_ZERO exception if rt = 0 |
| 06 | AND rd, rs, rt | R | bitwise AND, sets Z N |
| 07 | OR rd, rs, rt | R | bitwise OR, sets Z N |
| 08 | XOR rd, rs, rt | R | bitwise XOR, sets Z N |
| 09 | NOT rd, rs | R | rd = ~rs, sets Z N |
| 0A | SHL rd, rs, rt | R | rd = rs shifted left by rt |
| 0B | SHR rd, rs, rt | R | rd = rs shifted right by rt |
| 0C | MOV rd, rs | R | rd = rs |
| 0D | CMP rs, rt | R | computes rs - rt, sets flags only |
| 10 | ADDI rd, rs, imm | I | rd = rs + imm, sets Z N C V |
| 11 | LDI rd, imm | I | rd = imm |
| 12 | LW rd, imm(rs) | I | rd = mem[rs + imm] |
| 13 | SW rd, imm(rs) | I | mem[rs + imm] = rd |
| 14 | PUSH rd | I | SP = SP - 1, then mem[SP] = rd |
| 15 | POP rd | I | rd = mem[SP], then SP = SP + 1 |
| 16 | RET | I | PC = mem[SP], then SP = SP + 1 |
| 20 | JMP off | J | PC = PC + 1 + off |
| 21 | BEQ off | J | branch if Z = 1 |
| 22 | BNE off | J | branch if Z = 0 |
| 23 | BLT off | J | branch if N != V |
| 24 | BGE off | J | branch if N = V |
| 25 | CALL off | J | push PC+1, then PC = PC + 1 + off |

## Memory map (4096 words)

| Region | Range | Notes |
|---|---|---|
| Code | 0x000 - 0x7FF | program is loaded here, PC starts at 0x000 |
| Data | 0x800 - 0xBFF | globals and heap |
| Stack | 0xC00 - 0xFFF | initial SP = 0x1000 |

SP below 0xC00 is a stack overflow, SP above 0x1000 is a stack underflow.

## Exception codes

| Code | Name | Raised when |
|---|---|---|
| 0 | NONE | never, means no exception |
| 1 | STACK_OVERFLOW | PUSH/CALL would take SP below 0xC00 |
| 2 | STACK_UNDERFLOW | POP/RET would take SP above 0x1000 |
| 3 | INVALID_MEM_ACCESS | load/store outside 0x000-0xFFF, or a write into the code region |
| 4 | DIV_BY_ZERO | DIV with rt = 0 |
| 5 | ILLEGAL_INSTRUCTION | undefined opcode |
| 6 | PC_OUT_OF_RANGE | PC outside the code region |

## Module interface contract

Each module exposes only these signatures to the simulator core, so every module can be built
and tested on its own. Shared types live in `isa_spec/isa201.h`.

| Module | Signature |
|---|---|
| Assembler | `std::vector<uint32_t> assemble(const std::string& path)` |
| Disassembler | `std::string disassemble(uint32_t word)` |
| ALU | `AluResult execute(AluOp op, uint32_t a, uint32_t b)` |
| Control unit | `ControlSignals decode(uint32_t instr)` and `uint32_t getMicroPC()` |
| Memory safety | `ExceptionCode checkStack(uint32_t sp, bool isPush)` and `ExceptionCode checkMem(uint32_t addr, bool isWrite)` |
| Simulator | `void load(const std::vector<uint32_t>&)`, `void step()`, `void run()`, `void reset()` |

## Open points

1. No immediate shifts, no AND/OR immediates and no byte access in this version. Adding them
   later is cheap, but not after the freeze.
2. The write into the code region rule is a suggestion, drop it if self-modifying code is wanted.
3. Ask the TA whether deriving RISC201 from an existing RISC design is acceptable for the
   "custom ISA" requirement.
