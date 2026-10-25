# RISC201 Processor Simulator: System Overview & Architecture Audit

## 1. Executive Summary & Core Mission
The **RISC201** repository is an architectural simulator in C++17 for a 32-bit custom RISC processor. Its job is to:
1. Model the machine state (16 registers, 4096-word memory, stack, flags).
2. Execute the 27 instructions of the frozen RISC201 ISA.
3. Catch architectural exceptions (overflow, underflow, invalid memory access, illegal opcodes, division by zero).
4. Provide an interactive debugging shell (REPL) with breakpoints, watchpoints, and memory dumps.

---

## 2. File Interaction & Runtime Data Flow

Here is how the files actually interact during a simulation run:

```
[main.cpp]
    │  Instantiates Cpu and Cli, runs the REPL loop
    ▼
  [Cli] (cli.hpp / cli.cpp)
    │  Parses commands ("run", "step", "break", "mem", "regs")
    │  Formats output to std::ostream
    ├──> [Debugger] (debugger.hpp / debugger.cpp)
    │       │  Manages breakpoint addresses, memory watchpoints, and symbol map
    │       │  Wraps step/run to intercept target addresses before/after execution
    │       ▼
    └───> [Cpu] (cpu.hpp / cpu.cpp)
            │  Owns physical state: registers_[16], memory_[4096], pc_, sp_, flags_
            │  Owns hardware instances: alu_, control_, safety_, pipeline_
            │  Coordinates load(), step(), run(), reset()
            ▼
          [Pipeline] (pipeline.hpp / pipeline.cpp)
            │  Drives the 4-stage execution sequence sequentially
            │
            ├── [IF]    Reads cpu_.readMem(pc) into IfId latch
            │
            ├── [ID/OF] Calls control_.decode(instr) --> [ControlUnit]
            │           Reads cpu_.reg(rs), cpu_.reg(rt) into IdEx latch
            │
            ├── [EX]    Calls alu_.execute(...)       --> [Alu]
            │           Calls safety_.checkStack(...) --> [MemorySafety]
            │           Calls safety_.checkMem(...)   --> [MemorySafety]
            │           Computes branch/jump targets into ExWb latch
            │
            └── [WB]    Commits to cpu_:
                        - cpu_.setReg(rd, val)
                        - cpu_.writeMem(addr, val)
                        - cpu_.setFlags(flags)
                        - cpu_.setSp(nextSp)
                        - cpu_.setPc(target | pc + 1)
```

### Static Contracts vs Concrete Implementations

```
┌────────────────────────────────────────────────────────┐
│               isa_spec/isa201.h                        │
│ (Universal god-header: Opcodes, Constants, Formats,    │
│  ControlSignals, AluOp, Flags, Encoders, Decoders)     │
└────────────┬──────────────┬──────────────┬─────────────┘
             │              │              │
             ▼              ▼              ▼
       ┌───────────┐  ┌───────────┐  ┌───────────┐
       │  alu/     │  │ control_  │  │ memory_   │
       │  Alu      │  │ ControlUnit│ │ MemorySafety│  (the real modules)
       └─────┬─────┘  └─────┬─────┘  └─────┬─────┘
             │              │              │
             └──────────────┼──────────────┘
                            ▼
              Held by value in Cpu, injected
              into Pipeline by reference
```

---

## 3. Redundancy & Over-Engineering Audit

There is a significant amount of structural scaffolding and boilerplate in this codebase. Here is the honest breakdown of what is redundant, why it exists, and whether it matters:

### 1. Module Folders Hold The Implementation, Cpu Holds One Of Each
- **The Shape:** `alu/`, `control_unit/` and `memory_safety/` each define a concrete class (`Alu`, `ControlUnit`, `MemorySafety`) with no virtual methods. `Cpu` holds one of each by value and injects them into `Pipeline` by reference.
- **Why it exists:** Team scaffolding. The folders were laid out so members could work in isolation, and the pipeline only depends on the module's method signatures, not on how the module is built.
- **The Reality:** With a single implementation the injection buys flexibility rather than speed: swapping in a second implementation (for example a vertical microcode control unit) is a change in `Cpu` only.

### 2. The Assembler and Disassembler Are Separate Batch Tools
- **The Shape:** `assembler/` and `disassembler/` expose free functions, `assemble(path)` and `disassemble(path)`, with their own `risc201-asm` and `risc201-disasm` executables.
- **The Reality:** They are deliberately not linked into the simulator. They translate a whole file at a time, so the simulator loads their `.hex` output rather than calling into them.

### 3. The "Pipeline" That Doesn't Actually Pipeline
- **The Issue:**
  - `Pipeline` defines three explicit hardware latches: `IfId`, `IdEx`, `ExWb`.
  - In `Pipeline::step()`, execution is strictly sequential:
    `fetch(); decode(); execute(); writeBack();`
  - One instruction completes all four stages before the next instruction is fetched.
- **The Reality:**
  - Because instructions never overlap, the latches are not acting as pipeline barrier registers—they are just glorified temporary variable structs for parameter passing between helper functions.
  - **Verdict:** This is intentional scaffolding for future work (when 4-stage or 6-stage overlapping execution and hazard stalls are added), but today it is purely synchronous code disguised as hardware registers.

### 4. Cyclic Coupling Between `Cpu` and `Pipeline`
- **The Issue:**
  - `Cpu` owns `Pipeline`.
  - When initializing `Pipeline`, `Cpu` passes `*this` into `Pipeline`'s constructor.
  - `Pipeline` holds `Cpu&` and directly reaches into `Cpu` to call `cpu_.reg()`, `cpu_.setReg()`, `cpu_.readMem()`, `cpu_.writeMem()`, `cpu_.setPc()`, and `cpu_.setException()`.
- **The Reality:**
  - `Cpu` and `Pipeline` are the same entity split arbitrarily across two files. `Cpu` delegates `step()` to `Pipeline`, which immediately calls `Cpu` back to do all memory and register reads/writes. 
  - True datapath separation would have `Pipeline` own the register file and memory bus, or have `step()` return a state delta to `Cpu`.

### 5. Multi-Layer Stop & Exception Translation
- **The Issue:**
  - An exception occurs in `Pipeline::execute()`.
  - `Pipeline` calls `cpu_.setException(code)`.
  - `Cpu` stores `exception_` and halts.
  - `Debugger` polls `cpu_.exception()` and `cpu_.halted()`, translates that into `StopReason::Exception` or `StopReason::Halted`.
  - `Cli` calls `debugger_.stopReason()`, matches `StopReason`, translates it into console strings, and queries `cpu_.exception()` again to print the exception name.
- **The Reality:** 3 layers of state wrapping (`ExceptionCode` -> `Cpu` flags -> `StopReason` -> `Cli` console text) to report a simple halt condition.

### 6. Hand-Encoded Assembly in Every Single Test
- **The Issue:**
  - Every test file in `tests/` (`test_pipeline.cpp`, `test_full_isa.cpp`, `test_exceptions.cpp`, `test_cli.cpp`, `test_debugger.cpp`, `test_cpu.cpp`) hand-encodes machine code instructions one by one:
    ```cpp
    encodeI(Opcode::LDI, 1, 0, 42),
    encodeI(Opcode::ADDI, 2, 1, -1),
    encodeR(Opcode::ADD, 3, 1, 2)
    ```
- **The Reality:**
  - Because `assembler/` was left as an unimplemented abstract header, tests could not parse assembly text (`"LDI R1, 42"`). Over 500 lines of repetitive hand-assembly boilerplate exist across the test files.

---

## 4. Why Is It Structured This Way? (The Honest Context)

1. **Course Specification Constraints:** The author had to satisfy specific grading rubric items (e.g. "provide an ALU interface", "provide a Control Unit interface", "separate memory safety").
2. **"Stand-In" Bringup Strategy:** Rather than waiting for teammate modules, the core simulator was made functional first using minimal concrete stand-ins (`HardwiredControl`, `SimpleAlu`, `SimpleSafety`). Those have since been replaced by the real modules, which are what `Cpu` holds today.
3. **Prepared Extension Points:**
   - The pipeline latches are waiting for hazard detection and instruction overlapping.
   - `ControlUnit::getMicroPC()` is waiting for a vertical microcode engine.

---

## 5. Summary Table of Files & Roles

| File | Real Role | Necessary? | Redundancy Note |
|---|---|---|---|
| `isa_spec/isa201.h` | Core definitions, masks, encoders/decoders | **Yes** | Single source of truth for ISA constants |
| `alu/alu.hpp` | Structural ALU | **Yes** | Carry-lookahead adder, Booth multiply, restoring divide |
| `control_unit/control_unit.hpp` | Microcoded decode | **Yes** | Horizontal control ROM, one microword per instruction |
| `memory_safety/memory_safety.hpp` | Memory & stack checks | **Yes** | Bounds checks, exception strings, ASCII stack view |
| `assembler/assembler.hpp` | Two-pass assembler | **Yes** | Free functions behind the `risc201-asm` CLI |
| `disassembler/disassembler.hpp` | Disassembler | **Yes** | Free functions behind the `risc201-disasm` CLI |
| `simulator_core/cpu.*` | Architectural machine state | **Yes** | Real engine holding memory and registers |
| `simulator_core/pipeline.*` | 4-stage sequential executor | **Yes** | Synchronous today; latches exist for future hazards |
| `simulator_core/debugger.*` | Breakpoints & watchpoint manager | **Yes** | Clean separation of debugging features from CPU |
| `simulator_core/cli.*` | Interactive terminal shell | **Yes** | Command parser and REPL UI |
| `simulator_core/main.cpp` | CLI entry point | **Yes** | 30 lines, passes control to Cli |
