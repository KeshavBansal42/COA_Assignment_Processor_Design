> Branch-control update: conditional branches now use a two-bit predictor; only a wrong predicted next PC requires a conditional-branch flush. See `docs/cli.md` and `tests/test_branch_predictor.cpp`.

# Simulator core design

The simulator is `risc201::Cpu` in `simulator_core/`. Everything else in the core exists
to serve it: the pipelines execute instructions, the control unit decodes them, the ALU
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

## Three executors

`PipelineMode` selects which executor `step()` drives:

| Mode | Executor | One `step()` is |
|---|---|---|
| `Sequential` | `Pipeline` | one whole instruction (IF, ID/OF, EX, WB) |
| `FourStage` | `CyclePipeline(4)` | one clock cycle |
| `SixStage` | `CyclePipeline(6)` | one clock cycle |

The sequential pipeline is the reference model. The overlapped pipelines keep several
instructions in flight, forward results, stall on load-use/flags/SP/memory order and flush
on a taken branch. `tests/test_pipeline_hazards.cpp` proves all three reach the same
registers, memory, PC, SP, flags and exception.

`Cpu::nextPc()` reports the address the active pipeline will fetch next. It equals `pc()`
in sequential mode and the fetch stage's PC in the overlapped modes, where the last
committed write back is behind the fetch stage.

## The sequential reference (four stages)

One call to `Cpu::step()` moves one instruction through all four stages, in this order:

| Stage | What it does |
|---|---|
| IF | reads `mem[PC]` into the IF/ID latch |
| ID/OF | asks the control unit to decode the instruction, then reads the register operands into the ID/EX latch |
| EX | runs the ALU, works out the branch or jump target, and checks the stack and memory addresses |
| WB | commits the register write, the memory write, FLAGS, the new SP and the new PC |

The stages do not overlap here; the latches keep the stage boundaries explicit and are
the reference for the cycle pipeline.

## The overlapped pipelines

`CyclePipeline` is configured with 4 or 6 stages:

- 4 stage: IF, ID (decode and read operands), EX (ALU, branch resolve, address), WB
  (register, memory, flags, SP).
- 6 stage: IF, ID (decode), OF (read operands), EX (ALU, branch resolve), MEM (load,
  store), WB (register, flags, SP).

Each cycle the engine walks the stages oldest first, so a younger stage sees an older
stage's result in the same cycle. `lat_[i]` is the output of stage `i`.

### Hazards

- **Forwarding:** the operand stage takes a register value from the newest in-flight
  producer that has already computed it, otherwise from the register file. Back-to-back
  ALU operations therefore do not stall.
- **Load use:** a load's data is ready later (WB in the 4 stage, MEM in the 6 stage), so
  the dependent instruction waits one cycle.
- **Flags:** a branch waits while a flag-writing instruction is in flight, because flags
  are read as committed, not forwarded.
- **Stack pointer:** PUSH, POP, CALL and RET serialise on SP so the memory they touch is
  ordered.
- **Memory order:** two memory instructions never overlap.
- **Control:** a taken branch, jump, call or return in EX flushes the younger stages and
  redirects the fetch PC. The 6 stage flushes one more stage than the 4 stage.

### Stopping

HALT and exceptions are detected in EX. The offending instruction and everything younger
are killed, older instructions drain, then `PC` is set to the recorded stop address and
the machine is marked halted or faulted. A faulting instruction has no side effects.

## Retired instruction counter

`Cpu::instructions()` counts committed instructions. HALT commits normally in sequential
mode and is counted there; in the overlapped modes HALT never reaches write back, so it is
counted when the pipeline drains. A faulting instruction is not counted in any mode. This
keeps the totals identical across the three executors.

## Exceptions

An exception stops the machine and `run()` returns.

| Code | Raised in | When |
|---|---|---|
| `STACK_OVERFLOW` | EX | PUSH or CALL would take SP below the stack floor (`0xC00` fixed, or the heap high-water mark in dynamic mode) |
| `STACK_UNDERFLOW` | EX | POP or RET would take SP above `0x1000` |
| `INVALID_MEM_ACCESS` | EX | a load or store outside `0x000`-`0xFFF`, or a store into the code region |
| `DIV_BY_ZERO` | EX | DIV with a zero divisor, reported by the ALU |
| `ILLEGAL_INSTRUCTION` | ID/OF | the opcode is not a defined instruction |
| `PC_OUT_OF_RANGE` | IF | PC is past `0x7FF` |

## Modules

The executors take their collaborators by reference and never construct them:

- `Alu` (`alu/alu.hpp`) for arithmetic in EX
- `ControlUnit` (`control_unit/control_unit.hpp`) for decode in ID/OF
- `MemorySafety` (`memory_safety/memory_safety.hpp`) for the stack and memory checks in EX,
  including the optional dynamic stack whose floor follows the heap high-water mark

`Cpu` owns one of each: the structural `Alu`, the microcoded `ControlUnit` and
`MemorySafetyImpl`. Because they are injected rather than built inside the pipeline, a
different implementation (for example a vertical microcode control unit) can be swapped
in without touching the pipeline.

`Cpu::decodeInstruction()` and `Cpu::microTrace()` expose the control unit's decode and
its microPC walk for the CLI's `microtrace` command; they do not execute anything.

## Files

| File | Contents |
|---|---|
| `cpu.hpp` / `cpu.cpp` | machine state, `load`, `step`, `run`, `reset`, mode selection |
| `pipeline.hpp` / `pipeline.cpp` | the sequential four-stage reference and latches |
| `cycle_pipeline.hpp` / `cycle_pipeline.cpp` | the overlapped 4 and 6 stage engines |
| `debugger.hpp` / `debugger.cpp` | breakpoints, watchpoints and symbols |
| `cli.hpp` / `cli.cpp` | command shell |
| `main.cpp` | REPL entry point |

The ALU, control unit and safety module live in their own folders, `alu/`,
`control_unit/` and `memory_safety/`.

## Choices the ISA leaves open

- `SHR` is a logical shift, not an arithmetic one.
- DIV is unsigned restoring division; BLT/BGE independently use signed comparison
  flags. See [ALU contract](alu.md) for algorithms, flags, and boundary behavior.
- `MUL` reports signed overflow in V and leaves C clear, since a low-32-bit product has
  no carry.
- Shift amounts are taken modulo 32.
- Logic instructions commit Z/N and clear C/V. DIV, shifts, MOV, LDI, loads/stores,
  and floating-point instructions preserve architectural FLAGS. This records the
  current control policy; changing it needs team agreement.
- A store into the code region is rejected, as `docs/isa_spec.md` suggests. Dropping
  that rule is a one line change in the safety module if self-modifying code is wanted.
