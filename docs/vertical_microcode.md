# Vertical microcode

The control unit can read its signals from two encodings that describe the same
datapath:

- **Horizontal** (`control_unit/control_rom.hpp`): one 32-bit word per
  instruction, one bit per control wire, plus a small field for the ALU op.
  There is nothing to decode, but the word is wide.
- **Vertical** (`control_unit/vertical_microcode.hpp`): a narrow 16-bit word
  that names one micro operation, a small field for the ALU op, and a sequencer
  that walks a short microprogram per instruction.

`ControlUnit::setMode()` picks one; `Horizontal` is the default.

## Control word layout

| Encoding | Width | Fields |
|---|---|---|
| Horizontal | 32 bits | 13 one-bit control wires, 2 one-bit mux selects, 4-bit ALU op |
| Vertical | 16 bits | 4-bit micro op, 4-bit ALU op, 8 bits unused |

## Micro operations

A vertical word holds one `MicroOp`:

| Micro op | Signals it sets |
|---|---|
| `ALU_REG` | second operand from a register, and the ALU op field |
| `ALU_IMM` | second operand from the immediate, and the ALU op field |
| `REG_WRITE` | write the result back to rd |
| `FLAGS` | commit the ALU flags |
| `LOAD` | memory read into rd (also sets result from memory) |
| `STORE` | memory write from rd |
| `PUSH` | decrement SP and store |
| `POP` | load from SP and increment |
| `RET` | `PC = mem[SP]`, then SP increments |
| `JUMP` | `PC = PC + 1 + offset` |
| `BRANCH` | conditional `PC = PC + 1 + offset` |
| `HALT` | stop the machine |
| `END` | stop the sequencer, the instruction is done |

## Microprogram

`buildMicroProgram()` lays out one microprogram per opcode and records the start
index in a table indexed by opcode. Opcodes with no program decode to
`ILLEGAL_INSTRUCTION`. The programs end with `END`.

| Instruction | Microprogram | Length |
|---|---|---|
| `NOP` | `END` | 1 |
| `HALT` | `HALT END` | 2 |
| `ADD` `SUB` `MUL` | `ALU_REG(r) REG_WRITE FLAGS END` | 4 |
| `AND` `OR` `XOR` `NOT` | `ALU_REG(r) REG_WRITE FLAGS END` | 4 |
| `DIV` `SHL` `SHR` `MOV` | `ALU_REG(r) REG_WRITE END` | 3 |
| `FADD` `FSUB` `FMUL` `FDIV` | `ALU_REG(f) REG_WRITE END` | 3 |
| `CMP` | `ALU_REG(CMP) FLAGS END` | 3 |
| `ADDI` | `ALU_IMM(ADD) REG_WRITE FLAGS END` | 4 |
| `LDI` | `ALU_IMM(ADD) REG_WRITE END` | 3 |
| `LW` `SW` | `ALU_IMM(ADD) LOAD/STORE END` | 3 |
| `PUSH` `POP` `RET` | one stack op, then `END` | 2 |
| `JMP` | `JUMP END` | 2 |
| `BEQ` `BNE` `BLT` `BGE` | `BRANCH END` | 2 |
| `CALL` | `JUMP PUSH END` | 3 |

All 31 defined instructions fit in 90 of the 128 micro words.

## Sequencer

In vertical mode `ControlUnit::decode()` looks up `start[opcode]`, then walks the
micro words until `END`, folding each micro operation into the same
`ControlSignals` the horizontal ROM produces. Because the two encodings produce
identical signals, the pipelines are unaffected by the choice.

- `getMicroPC()` returns the start index of the current instruction's
  microprogram (the horizontal mode returns the ROM row, which is the opcode).
- `lastMicroOpCount()` returns how many micro operations ran: the microprogram
  length minus the `END`, or one for a horizontal word.
- `trace(instr)` returns every `MicroTraceEntry` the instruction walks, with the
  microPC, control word, micro operation, ALU op and an end marker. It does not
  change the decode state.

The whole microprogram for one machine instruction is folded in a single
`decode()` call: the datapath has no micro-cycle stalls, so a micro operation
does not advance the pipeline clock on its own. The microPC walk is exposed for
inspection through `trace()` / the CLI `microtrace` command rather than being
interleaved with instruction execution. This is a modelling choice, not a
hardwired-control shortcut: every control signal still comes from control
memory.

The CLI uses `trace()` for `microtrace [address]`, which prints the microPC
stepping through the control store and the decoded signals, and reports
`ILLEGAL_INSTRUCTION` for an undefined opcode:

    > micro vertical
    > microtrace 3
    microtrace PC 0x00000003  word 0x08cc4000  ADD [vertical]
      microPC 0x0003  word 0x00000001  ALU_REG alu=ADD
      microPC 0x0004  word 0x00000003  REG_WRITE
      microPC 0x0005  word 0x00000004  FLAGS
      microPC 0x0006  word 0x00000000  END  (sequencer stop)
      decoded signals: regWrite writeFlags  aluSrc=register  resultSrc=alu

`tests/test_microcode.cpp` checks that every defined opcode, plus an undefined
one, decodes to the same signals in both encodings. `tests/test_cli.cpp` checks
the microPC trace itself.
