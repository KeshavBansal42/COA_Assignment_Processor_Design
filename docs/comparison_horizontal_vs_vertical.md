# Horizontal vs vertical microcode

Both encodings drive the same datapath and, because the sequential and
overlapped pipelines only ever call `ControlUnit::decode()`, both give the same
cycles for the same program (see `docs/evaluation_matrix.md`: the horizontal and
vertical rows of a program match on cycles, CPI, stalls and flushes). The choice
is therefore about the control store and the decode logic, not throughput in
this model.

## Control word

| Encoding | Width | Per instruction | Decode |
|---|---|---|---|
| Horizontal | 32 bits, one bit per control wire | one word | table lookup, no state |
| Vertical | 16 bits, a named micro operation | a short microprogram | sequencer plus micro op decoder |

## Control store the programs read

`risc201-eval` counts the control words and bits each run reads (one 32-bit word
per instruction for horizontal, the microprogram length in 16-bit words for
vertical):

| Program | Horizontal words | Vertical words | Horizontal bits | Vertical bits |
|---|---|---|---|---|
| loop | 8 | 24 | 256 | 384 |
| recursion | 14 | 38 | 448 | 608 |
| stack_fault | 2 | 6 | 64 | 96 |
| hazard_raw | 7 | 24 | 224 | 384 |
| hazard_load_use | 6 | 18 | 192 | 288 |
| hazard_branch | 7 | 19 | 224 | 304 |

## Reading

- The vertical word is half the width, but a machine instruction needs three or
  four micro operations, so the **total control store is larger** than the
  horizontal one for this ISA. Vertical microcode pays off when instructions are
  long and complex, where one wide word per instruction is wasteful; RISC201's
  instructions are short and mostly one ALU operation, so the narrow word does
  not make up for the extra words.
- The horizontal word is wider but needs **no sequencer**: `decode()` is one
  indexed read. The vertical path needs a micro PC, an end test and a fold of
  each micro operation into the control signals, which is the cost a real design
  would pay in control logic and latency.
- The vertical format is **easier to extend**. Adding behaviour means adding a
  micro operation and a word; the horizontal format means adding a wire bit and
  revisiting every word that should drive it. This is the tradeoff microprogramming
  was invented for.
- Both encodings are held to the same contract by `tests/test_microcode.cpp`,
  which asserts they decode all 27 opcodes to identical signals.

## Where vertical would win here

`MUL` and `DIV` are a single micro operation each today because the ALU is
structural. A design that expanded them into repeated add and shift micro
operations would show the vertical advantage: one narrow `add`, `shift` and
`repeat` microprogram instead of a wide dedicated word per step.
