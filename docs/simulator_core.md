# Simulator core design

The simulator is `risc201::Cpu` in `simulator_core/`. Everything else in the core exists
to serve it: the pipeline executes instructions, the control unit decodes them, the ALU
does the arithmetic and the safety module checks addresses.

## Machine state

`Cpu` holds the machine described in `docs/isa_spec.md`:

- 16 general purpose 32-bit registers, `R0` to `R15`
- `PC`, the address of the next instruction
- `SP`, the full-descending stack pointer, starting at `0x1000`
- `FLAGS`, the Z, N, C and V bits
- 4096 words of word-addressed memory
- a halt flag and an exception code

`load()` clears memory, copies the program into the code region and resets. `reset()`
puts the registers, PC, SP, FLAGS, halt flag and exception back to their start values and
leaves memory alone, so a program survives a reset and can be run again.

## The four stages

One call to `Cpu::step()` moves one instruction through all four stages, in this order:

| Stage | What it does |
|---|---|
| IF | reads `mem[PC]` into the IF/ID latch |
| ID/OF | asks the control unit to decode the instruction, then reads the register operands into the ID/EX latch |
| EX | runs the ALU, works out the branch or jump target, and checks the stack and memory addresses |
| WB | commits the register write, the memory write, FLAGS, the new SP and the new PC |

The stages do not overlap: an instruction finishes before the next one starts. That is
deliberate for now, because overlapping them would create read-after-write hazards and
handling those is later work. The three latches (`IfId`, `IdEx`, `ExWb`) keep the stage
boundaries in place so the stages can be overlapped without restructuring anything.

## PC is updated in write back

`fetch()` reads the instruction but does not move PC. Write back commits either the
branch or jump target or `PC + 1`. This is what lets an exception leave PC pointing at
the instruction that faulted rather than the one after it, and it is why HALT leaves PC
on the HALT.

## Exceptions

An exception stops the machine. The pipeline stops before the stages that would have
written anything, so a faulting instruction has no side effects, and `run()` returns.

| Code | Raised in | When |
|---|---|---|
| `STACK_OVERFLOW` | EX | PUSH or CALL would take SP below `0xC00` |
| `STACK_UNDERFLOW` | EX | POP or RET would take SP above `0x1000` |
| `INVALID_MEM_ACCESS` | EX | a load or store outside `0x000`-`0xFFF`, or a store into the code region |
| `DIV_BY_ZERO` | EX | DIV with a zero divisor, reported by the ALU |
| `ILLEGAL_INSTRUCTION` | ID/OF | the opcode is not one of the 27 defined instructions |
| `PC_OUT_OF_RANGE` | IF | PC is past `0x7FF` |

## Modules and stand-ins

The pipeline talks to the module interfaces, never to a concrete module:

- `Alu` (`alu/alu.hpp`) for arithmetic in EX
- `ControlUnit` (`control_unit/control_unit.hpp`) for decode in ID/OF
- `MemorySafety` (`memory_safety/memory_safety.hpp`) for the stack and memory checks in EX

The real modules do not exist yet, so `simulator_core/` provides `HardwiredControl`,
`SimpleAlu` and `SimpleSafety`. They implement those interfaces, so replacing them with
the real control unit, ALU and safety modules is a change in `Cpu` only.

## Files

| File | Contents |
|---|---|
| `cpu.hpp` / `cpu.cpp` | machine state, `load`, `step`, `run`, `reset` |
| `pipeline.hpp` / `pipeline.cpp` | the four stages and the latches |
| `hardwired_control.hpp` / `.cpp` | stand-in decode for all 27 instructions |
| `simple_alu.hpp` / `simple_alu.cpp` | stand-in ALU for all 12 operations |
| `simple_safety.hpp` / `simple_safety.cpp` | stand-in stack and memory checks |
| `debugger.hpp` / `debugger.cpp` | breakpoints, watchpoints and symbols |
| `cli.hpp` / `cli.cpp` | command shell |
| `main.cpp` | REPL entry point |

## Choices the ISA leaves open

- `SHR` is a logical shift, not an arithmetic one.
- Division is signed, to match the signed `BLT` and `BGE` comparisons.
- `MUL` reports signed overflow in V and leaves C clear, since a low-32-bit product has
  no carry.
- Shift amounts are taken modulo 32.
- A store into the code region is rejected, as `docs/isa_spec.md` suggests. Dropping
  that rule is a one line change in the safety module if self-modifying code is wanted.
