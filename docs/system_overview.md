# RISC201 system overview

RISC201 is a C++17 simulator for a 32-bit custom RISC processor. It is the
CSC-201 Computer Organization & Architecture assignment implementation: two
pipeline organisations (4 and 6 stage), a microprogrammed control unit that can
read horizontal or vertical control memory, a structural ALU, a two-pass
assembler with a stack macro preprocessor, a disassembler, a debugger CLI, a
runtime exception framework and stack visualisation.

## What runs at a glance

```
 main.cpp
   |  owns Cpu and Cli, runs the REPL
   v
 Cli ──> Debugger ──> Cpu
                        ├── Pipeline          sequential reference (IF/ID/EX/WB)
                        ├── CyclePipeline(4)  overlapped 4 stage
                        ├── CyclePipeline(6)  overlapped 6 stage
                        ├── Alu               CLA, Wallace, restoring, binary32
                        ├── ControlUnit       horizontal ROM or vertical microprogram
                        └── MemorySafety      stack and address checks
```

`Cpu` holds the architectural state: 16 registers, 4096 words of memory, PC, SP,
FLAGS, the halt flag and the current exception. It owns one instance of each
module by value and injects them into the executors by reference.

## Module map

| Path | Role |
|---|---|
| `isa_spec/isa201.h` | frozen contract: opcodes, formats, encoders/decoders, flags, exceptions, control signals |
| `assembler/` | stack preprocessor, two-pass assembler, `risc201-asm` CLI |
| `disassembler/` | word and file disassembly, `risc201-disasm` CLI |
| `alu/` | carry-lookahead adder, Wallace/Booth multiplication, restoring and Newton-Raphson division, binary32 |
| `control_unit/` | horizontal control ROM, vertical microprogram, sequencer, microPC trace |
| `memory_safety/` | stack/address checks, exception strings, ASCII stack frame renderer |
| `simulator_core/` | CPU, sequential pipeline, overlapped cycle pipelines, debugger, CLI |
| `visualizer/` | animated code/register/stack/pipeline view, `risc201-viz` |
| `eval/` | pipeline × microcode evaluation harness, `risc201-eval` |
| `tests/` | plain `assert()` unit and integration suites, demo/hazard programs |
| `docs/` | ISA, design, datapath, comparison and evaluation documents |

## Execution path of one command

A CLI command such as `run` goes through `Debugger`, which checks breakpoints
against `Cpu::nextPc()` and watchpoints against the last memory write. `Cpu::step()`
then either runs one instruction on the sequential reference or advances one clock
cycle on the selected overlapped pipeline. During those stages the pipeline calls
`ControlUnit::decode`, `Alu::execute` and the `MemorySafety` checks, then commits
the register file, memory, FLAGS, SP and PC.

Programs enter the machine in two ways:

- `risc201-asm` preprocesses stack macros, assembles two passes and writes a
  `.hex` file, which `risc201-sim` or `risc201-viz` loads.
- the visualizer also accepts a `.s` file and assembles it in memory.

## Assignment coverage

| Requirement | Where |
|---|---|
| 4-stage and 6-stage RISC201, behaviour compared | `simulator_core/cycle_pipeline.cpp`, `docs/datapath_{4,6}_stage.md`, `docs/comparison_4_vs_6_stage.md` |
| Microprogrammed control unit, horizontal and vertical | `control_unit/`, `docs/vertical_microcode.md`, `docs/comparison_horizontal_vs_vertical.md` |
| Efficient ALU for arithmetic/shift/logical | `alu/`, `docs/alu.md` |
| Two-pass assembler | `assembler/assembler.cpp`, `docs/assembler_plan.md` |
| Disassembler | `disassembler/disassembler.cpp` |
| Interactive CLI for registers, execution, pipeline | `simulator_core/cli.cpp`, `docs/cli.md` |
| Step-by-step simulator with debugger | `simulator_core/debugger.cpp`, `simulator_core/cli.cpp` |
| Stack preprocessor macro expansion | `assembler/preprocessor.cpp`, `docs/preprocessor.md` |
| Stack boundary checks and runtime exceptions | `memory_safety/`, `docs/memory_safety_interaction.md` |
| ASCII stack memory frame renderer | `memory_safety` `renderStackAscii`, `visualizer/` |
| Microcode drives the CPU, microPC display | `control_unit/`, CLI `microtrace`, `docs/vertical_microcode.md` |
| ALU drives all arithmetic/shift/logical ops | `ControlSignals::aluOp` from both control encodings |
| Evaluation across pipelines, microcode and programs | `eval/eval_matrix.cpp`, `docs/evaluation_matrix.md` |

## Build and test

```
cmake -S . -B build
cmake --build build
cd build && ctest --output-on-failure
```

The suite covers the ISA header, ALU (integer and binary32), memory checks,
control unit, horizontal/vertical microcode equality, CPU, sequential pipeline,
overlapped pipeline parity, full ISA, exceptions, CLI, debugger, assembler,
disassembler, visualizer and integration. The evaluation run is also registered
as a smoke test that every program still runs in every configuration.
