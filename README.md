# RISC201

A RISC201 simulator in C++17: the full ISA running on a 4-stage pipeline, driven by a
command line shell with a breakpoint and watchpoint debugger.

CSC-201 Computer Organization & Architecture, Autumn 2026.

## Layout

| Path | Contents |
|---|---|
| `isa_spec/isa201.h` | the frozen ISA: opcodes, instruction formats, encoding helpers, exception codes and the types the modules share |
| `simulator_core/` | CPU, pipeline, CLI and debugger |
| `alu/` | ALU interface |
| `control_unit/` | control unit interface |
| `assembler/` | assembler interface |
| `disassembler/` | disassembler interface |
| `memory_safety/` | stack and memory range check interface |
| `tests/` | unit and integration tests, `tests/programs/` holds the demo programs |
| `docs/` | ISA specification and design documents |

## Build

    cmake -S . -B build
    cmake --build build

## Run

    ./build/risc201-sim
    ./build/risc201-sim tests/programs/loop.hex

Type `help` inside the simulator for the command list, or read `docs/cli.md`.

## Test

    cd build && ctest --output-on-failure

## Status

The simulator core is done: the whole ISA executes on the 4-stage pipeline, exceptions
are raised and reported, and the CLI and debugger work. The three demo programs in
`tests/programs/` run end to end, one of them on purpose into a stack overflow.

The ALU, control unit and memory safety modules are not written yet, so the simulator
uses the stand-ins in `simulator_core/`. They implement the same interfaces as the real
modules, so swapping them in is a change in `Cpu` only.

Not started: vertical microcode, the 6-stage pipeline, and the evaluation work.
