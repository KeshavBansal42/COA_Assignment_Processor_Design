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
            ├── [ID/OF] Calls control_.decode(instr) --> [HardwiredControl]
            │           Reads cpu_.reg(rs), cpu_.reg(rt) into IdEx latch
            │
            ├── [EX]    Calls alu_.execute(...)       --> [SimpleAlu]
            │           Calls safety_.checkStack(...) --> [SimpleSafety]
            │           Calls safety_.checkMem(...)   --> [SimpleSafety]
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
       │  alu.hpp  │  │ unit.hpp  │  │ safety.hpp│  (Pure abstract interfaces)
       └─────┬─────┘  └─────┬─────┘  └─────┬─────┘
             │              │              │
      implements     implements     implements
             │              │              │
             ▼              ▼              ▼
       ┌───────────┐  ┌───────────┐  ┌───────────┐
       │SimpleAlu  │  │Hardwired  │  │Simple     │
       │           │  │Control    │  │Safety     │  (Concrete "stand-in" modules)
       └─────┬─────┘  └─────┬─────┘  └─────┬─────┘
             │              │              │
             └──────────────┼──────────────┘
                            ▼
              Stored directly inside Cpu
```

---

## 3. Redundancy & Over-Engineering Audit

There is a significant amount of structural scaffolding and boilerplate in this codebase. Here is the honest breakdown of what is redundant, why it exists, and whether it matters:

### 1. The Interface-Folder vs Stand-in Duplication (Highest Redundancy)
- **The Issue:** 
  - `alu/alu.hpp`, `control_unit/control_unit.hpp`, and `memory_safety/memory_safety.hpp` declare pure abstract base classes (`Alu`, `ControlUnit`, `MemorySafety`) with virtual destructors and pure virtual methods (`= 0`).
  - `simulator_core/` has `simple_alu.hpp`, `hardwired_control.hpp`, and `simple_safety.hpp`, which implement those exact interfaces.
  - There is only **one** implementation of each in the entire system.
- **Why it exists:** Academic team scaffolding. The project was set up so different team members could code their modules in separate folders without touching `simulator_core/`.
- **The Reality:** Until someone actually writes a secondary module (e.g., a microcoded control unit), these virtual base classes are pure overhead. In `Cpu`, they aren't even allocated dynamically—`Cpu` instantiates `HardwiredControl control_{}; SimpleAlu alu_{}; SimpleSafety safety_{};` directly as value members on the stack/object.

### 2. The Floating "Abstract" Assembler and Disassembler
- **The Issue:** 
  - `assembler/assembler.hpp` defines `class Assembler { virtual vector<uint32_t> assemble(...) = 0; };`
  - `disassembler/disassembler.hpp` defines `class Disassembler { virtual string disassemble(...) = 0; };`
- **The Reality:** 
  - Neither of these is connected to the simulator. Nothing in `simulator_core/` calls, links, or references them.
  - An assembler and disassembler are stateless batch translation functions (`string -> vector<uint32_t>` and `uint32_t -> string`). Wrapping them in abstract polymorphic classes with vtables is classic OOP Java-brain over-engineering.

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
2. **"Stand-In" Bringup Strategy:** Rather than waiting for teammate modules, the core simulator was made functional first using minimal concrete stand-ins (`HardwiredControl`, `SimpleAlu`, `SimpleSafety`).
3. **Prepared Extension Points:**
   - The pipeline latches are waiting for hazard detection and instruction overlapping.
   - `ControlUnit::getMicroPC()` is waiting for a vertical microcode engine.
   - `Assembler` and `Disassembler` headers are waiting for their implementations.

---

## 5. Summary Table of Files & Roles

| File | Real Role | Necessary? | Redundancy Note |
|---|---|---|---|
| `isa_spec/isa201.h` | Core definitions, masks, encoders/decoders | **Yes** | Single source of truth for ISA constants |
| `alu/alu.hpp` | Abstract interface for ALU | Optional | Pure interface with 1 implementation (`SimpleAlu`) |
| `control_unit/control_unit.hpp` | Abstract interface for decode | Optional | Pure interface with 1 implementation (`HardwiredControl`) |
| `memory_safety/memory_safety.hpp` | Abstract interface for checks | Optional | Pure interface with 1 implementation (`SimpleSafety`) |
| `assembler/assembler.hpp` | Abstract interface for assembler | **Redundant** | An assembler should be a function, not a virtual class |
| `disassembler/disassembler.hpp` | Abstract interface for disassembler | **Redundant** | Disassembly is a stateless function |
| `simulator_core/cpu.*` | Architectural machine state | **Yes** | Real engine holding memory and registers |
| `simulator_core/pipeline.*` | 4-stage sequential executor | **Yes** | Synchronous today; latches exist for future hazards |
| `simulator_core/simple_alu.*` | Working ALU logic | **Yes** | Implements all 12 math/logic operations |
| `simulator_core/hardwired_control.*` | Working instruction decoder | **Yes** | Decodes all 27 instructions to control signals |
| `simulator_core/simple_safety.*` | Memory & stack bounds checking | **Yes** | Implements all hardware protection rules |
| `simulator_core/debugger.*` | Breakpoints & watchpoint manager | **Yes** | Clean separation of debugging features from CPU |
| `simulator_core/cli.*` | Interactive terminal shell | **Yes** | Command parser and REPL UI |
| `simulator_core/main.cpp` | CLI entry point | **Yes** | 30 lines, passes control to Cli |
