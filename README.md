# RISC201

A RISC201 simulator in C++17: the full ISA on a selectable pipeline (sequential,
4-stage or 6-stage, the last two overlapped with forwarding, hazard stalls and
branch flushes), driven by a command line shell with a breakpoint and watchpoint
debugger, plus an animated code and machine state visualizer.

CSC-201 Computer Organization & Architecture, Autumn 2026.

## Layout

| Path | Contents |
|---|---|
| `isa_spec/isa201.h` | the frozen ISA: opcodes, instruction formats, encoding helpers, exception codes and the types the modules share |
| `simulator_core/` | CPU, the sequential and overlapped pipelines, CLI and debugger |
| `alu/` | structural ALU: carry-lookahead adder, Booth multiply, restoring divide |
| `control_unit/` | microcoded control unit, horizontal ROM and vertical microprogram |
| `assembler/` | two-pass assembler and the `risc201-asm` CLI |
| `disassembler/` | disassembler and the `risc201-disasm` CLI |
| `memory_safety/` | stack and memory range checks, exception strings, ASCII stack view |
| `visualizer/` | the animated code and machine state view, `risc201-viz` |
| `eval/` | the pipeline and microcode evaluation harness, `risc201-eval` |
| `tests/` | unit and integration tests, `tests/programs/` holds the demo programs |
| `docs/` | ISA specification, design and evaluation documents |

## Build

    cmake -S . -B build
    cmake --build build

## Run

    ./build/risc201-sim
    ./build/risc201-sim tests/programs/loop.hex

Type `help` inside the simulator for the command list, or read `docs/cli.md`.
`mode` picks the pipeline, `micro` picks the control encoding and `cycles` shows
the counters.

The visualizer animates a program a cycle at a time and shows the code line, the
registers, the stack and the pipeline:

    ./build/risc201-viz tests/programs/loop.hex --mode 6stage
    ./build/risc201-viz tests/programs/recursion.s --micro vertical

The evaluation harness prints the pipeline and microcode comparison numbers:

    ./build/risc201-eval docs/evaluation_matrix.md

## Test

    cd build && ctest --output-on-failure

## Status

Complete. The whole ISA runs on the sequential reference and on the 4-stage and
6-stage overlapped pipelines, which reach the same machine state as the
reference (`tests/test_pipeline_hazards.cpp`). The control unit reads either the
horizontal ROM or the vertical microprogram, and both decode identically
(`tests/test_microcode.cpp`). Exceptions are raised and reported, the CLI and
debugger work, the assembler and disassembler round-trip, and the visualizer and
evaluation harness are in place.

The real modules are wired into `Cpu`: the structural ALU (`alu/`), the
microcoded control unit (`control_unit/`), the memory safety checks
(`memory_safety/`) and the assembler/disassembler. `Cpu` passes them to the
pipeline by reference, so swapping an implementation is a change in `Cpu` only.

The evaluation, the pipeline comparison, the microcode comparison and the
datapath diagrams live in `docs/`.
