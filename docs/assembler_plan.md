# RISC201 Assembler Implementation Plan

## 1. Overview
The assembler is a standalone translation engine that converts a RISC201 assembly source file (`.s`) into machine code, maintains a persistent vector of encoded 32-bit instruction words, and outputs a formatted `.hex` file.

```
"path/to/prog.s" ───► [assemble()] ───► outputs "path/to/prog.hex"
                             │
                             └──► std::vector<uint32_t>& outProgram
```

### Constraints & Architectural Contracts
- **Function-Based Interface:** No abstract classes or virtual tables. Exposed in `assembler/assembler.hpp` as:
  ```cpp
  namespace risc201 {
  // Assembles .s file, populates outProgram, and writes pure hex machine code to <dir>/<stem>.hex
  void assemble(const std::string& path, std::vector<uint32_t>& outProgram);

  // Convenience overload that assembles and writes <dir>/<stem>.hex
  void assemble(const std::string& path);
  }
  ```
- **Strict Uppercase Casing:** Mnemonics (`ADD`, `LDI`, `SW`) and registers (`R0`–`R15`) are **strictly case-sensitive and must be in uppercase**. Any lowercase or mixed-case mnemonic/register (e.g. `add`, `r1`) is rejected with a line-numbered `std::runtime_error`.
- **Pure Machine Code Output:** The generated `.hex` file contains only bare 8-digit hexadecimal words (`%08x`), one per line. No mnemonics, comments, or annotations.
- **Output Placement:** Output `.hex` file is placed in the **exact same directory** as the input `.s` file (e.g., `tests/programs/loop.s` $\rightarrow$ `tests/programs/loop.hex`).
- **No Data Directives:** Pure instruction stream only. Directives like `.data`, `.word`, or `.space` are not supported.
- **Stack Preprocessor:** `.macro` / `.endmacro` definitions are expanded before Pass 1 (`assembler/preprocessor.cpp`). This is how multi-instruction full-descending stack sequences are expressed; see `docs/preprocessor.md`.
- **Labels & Numbers:** `JMP`, `BEQ`, `BNE`, `BLT`, `BGE` and `CALL` take a **label**; raw numeric PC offsets (`BNE -4`) are rejected and the offset is computed as `target - (PC + 1)`. Immediates for `ADDI`, `LDI`, `LW` and `SW` do accept decimal or `0x` hex numbers.
- **Label Placement:** Labels must be on a standalone line (`loop:` then the instruction). Inline prefixes (`loop: ADD ...`) are rejected.
- **Error Reporting:** All syntax, range, or file errors throw `std::runtime_error` containing the 1-based source line number.
- **ISA Reuse:** Directly uses `Opcode`, `kInstructionTable`, `lookupInstruction`, `encodeR`, `encodeI`, `encodeJ` from `isa_spec/isa201.h`.

---

## 2. Architecture & Data Flow

```
                      ┌────────────────────────┐
                      │   Assembly File (.s)   │
                      └───────────┬────────────┘
                                  │
                                  ▼
                      ┌────────────────────────┐
                      │ Pass 1: Label Scanner  │
                      │ - Strip ; / # comments │
                      │ - Track line numbers   │
                      │ - Map labels -> addrs  │
                      └───────────┬────────────┘
                                  │
                                  ▼
                      ┌────────────────────────┐
                      │ Pass 2: Line Parser    │
                      │ - Check strict UPPER   │
                      │ - Tokenize mnemonic    │
                      │ - Parse R/I/J formats  │
                      │ - Resolve labels       │
                      └───────────┬────────────┘
                                  │
                                  ▼
                      ┌────────────────────────┐
                      │ Persistent Word Vector │
                      │ std::vector<uint32_t>  │
                      └───────────┬────────────┘
                                  │
                                  ▼
                      ┌────────────────────────┐
                      │    writeHexFile()      │
                      │ Outputs bare hex words │
                      │ <dir>/<stem>.hex       │
                      └────────────────────────┘
```

---

## 3. Implementation Phases

### Phase 0: Stack Macro Preprocessing
1. Read the whole file into `SourceLine { text, line }` records.
2. Run `preprocess()` (`assembler/preprocessor.cpp`): collect `.macro` / `.endmacro`
   definitions, then expand each invocation, substituting `\name` and `\1`
   parameters. The assembler passes below only see the expanded instruction
   stream. See `docs/preprocessor.md`.

### Phase 1: Line Ingestion & Label Scanning (Pass 1)
1. Iterate the preprocessed `SourceLine` records.
2. For each line:
   - Strip comments starting with `;` or `#`.
   - Trim leading and trailing whitespace.
   - If line is empty, skip.
   - A label is a whole line matching `^[a-zA-Z_][a-zA-Z0-9_]*:$`.
   - Record `symbolTable[label] = instructionCount` and continue to the next
     line. An inline instruction after the label is rejected.

### Phase 2: Tokenization & Strict Parsing Helpers
Helper functions in `assembler/assembler.cpp`:
- **`parseRegister(token, line) -> uint8_t`:**
  - Must match strictly `^R([0-9]|1[0-5])$` (`R0` through `R15`).
  - Rejects lowercase `r1` or out-of-range (`R16`) with `std::runtime_error("Line <N>: invalid register '<token>'")`.
- **`parseImmediate(token, line) -> int32_t`:**
  - Parses a signed decimal (e.g. `-42`, `12345`) or an unsigned `0x` hex value.
  - Validates the 18-bit signed immediate range and sign-extends the 18-bit field.
- **`parseBranchTarget(token, line, symbols, currentPC) -> int32_t`:**
  - Requires a label: a numeric offset is rejected with
    `"manual PC offset not allowed"`.
  - Computes `offset = targetAddr - (currentPC + 1)` and validates the 26-bit
    signed range.
  - An unknown label throws `"undefined label '<token>'"`.
- **`parseMemOperand(token, line, ...) -> pair<uint8_t rs, int32_t imm>`:**
  - Strictly parses `imm(RS)` (e.g. `4(R2)` or `0(R1)`).

### Phase 3: Format-Specific Parsers (Pass 2)
Dispatches based on `info->format` from `lookupInstruction(mnemonic)`:

| Format | Mnemonic(s) | Syntax (Strict Uppercase) | Encoding Call |
|---|---|---|---|
| **None** | `NOP`, `HALT` | `NOP`, `HALT` | `encodeOpcode(opcode)` |
| **R-Type (3-reg)** | `ADD`, `SUB`, `MUL`, `DIV`, `AND`, `OR`, `XOR`, `SHL`, `SHR` | `OP RD, RS, RT` | `encodeR(opcode, rd, rs, rt)` |
| **R-Type (2-reg)** | `NOT`, `MOV` | `NOT RD, RS`<br>`MOV RD, RS` | `encodeR(opcode, rd, rs, 0)` |
| **R-Type (cmp)** | `CMP` | `CMP RS, RT` | `encodeR(Opcode::CMP, 0, rs, rt)` |
| **I-Type (arith)** | `ADDI` | `ADDI RD, RS, imm` | `encodeI(Opcode::ADDI, rd, rs, imm)` |
| **I-Type (ldi)** | `LDI` | `LDI RD, imm` | `encodeI(Opcode::LDI, rd, 0, imm)` |
| **I-Type (mem)** | `LW`, `SW` | `LW RD, imm(RS)`<br>`SW RD, imm(RS)` | `encodeI(opcode, rd, rs, imm)` |
| **I-Type (stack)** | `PUSH`, `POP` | `PUSH RD`<br>`POP RD` | `encodeI(opcode, rd, 0, 0)` |
| **I-Type (ret)** | `RET` | `RET` | `encodeI(Opcode::RET, 0, 0, 0)` |
| **R-Type (float)** | `FADD`, `FSUB`, `FMUL`, `FDIV` | `OP RD, RS, RT` | `encodeR(opcode, rd, rs, rt)` |
| **J-Type** | `JMP`, `BEQ`, `BNE`, `BLT`, `BGE`, `CALL` | `OP label` | `encodeJ(opcode, offset)` |

Every encoded instruction word is appended directly to the persistent `std::vector<uint32_t>`.

### Phase 4: Hex Output Generation & Code Size Validation
- Ensure `instructions.size() <= kCodeLimit + 1` (2048 words / `0x800`). Throw `std::runtime_error` if exceeded.
- Compute the output filename: derive `<dir>/<stem>.hex` from `<dir>/<stem>.s`.
- Call helper function:
  ```cpp
  void writeHexFile(const std::string& outputPath, const std::vector<uint32_t>& program);
  ```
  Writes pure machine code: one 8-character hex word per line (`%08x`), no comments or mnemonics.

### Phase 5: Verification & Tests
Create `tests/test_assembler.cpp` covering:
1. **Case-Sensitivity Checks:** Verify `ADD R1, R2, R3` succeeds, but `add r1, r2, r3` and `ADD r1, R2, R3` throw line-numbered errors.
2. **Instruction Set Coverage:** Assemble all 27 base instructions and compare against known `encodeR/I/J` values; the four floating-point instructions are covered by `tests/test_float.cpp` and `tests/programs/float_demo.s`.
3. **Labels:** Test backward loops (`BNE loop`) and forward calls (`CALL helper`); assert that a literal offset (`BNE -4`) and an inline label are rejected.
4. **Hex File Output:** Verify `<stem>.hex` is created beside the `.s` file containing pure 8-digit hex words, and verify it can be loaded directly by `Cpu::load` / `risc201-sim`.
5. **Round-Trip Test:** Verify that assembling demo programs produces the exact binaries in `tests/programs/*.hex`.
6. **Stack Macros:** Verify `SAVE` / `RESTORE` expand to the expected `PUSH` / `POP` words, plus wrong-argument-count, missing-`.endmacro` and unknown-parameter errors (see `docs/preprocessor.md`).

---

## 4. Resolved Architectural Decisions

All open questions have been confirmed by the prompter:

1. **Directives:** No data directives (`.data`, `.word`, etc.) — pure instructions only. `.macro` / `.endmacro` are the one preprocessing directive pair.
2. **Jump / Branch Targets:** Labels only (`BNE loop`); raw numeric offsets are rejected.
3. **Label Placement:** Standalone lines only (`loop:` then the instruction); inline prefixes are rejected.
4. **Case Sensitivity:** Strictly uppercase only (`ADD`, `R0`..`R15`). Lowercase mnemonics and registers are syntax errors.
5. **Architecture:** Pure function-based design without abstract base classes or virtual tables.
6. **Programmatic Access:** Provided via `void assemble(const std::string& path, std::vector<uint32_t>& outProgram)`.
7. **File Placement:** Output `.hex` file is placed in the same directory as the `.s` file.
8. **Hex File Content:** Pure machine code (8 hex digits per line), with no comments or mnemonics.
