> Branch-control update: conditional branches now use a two-bit predictor; only a wrong predicted next PC requires a conditional-branch flush. See `docs/cli.md` and `tests/test_branch_predictor.cpp`.

# 6 stage datapath

Implemented by `CyclePipeline` with `stages = 6` (see `simulator_core/cycle_pipeline.cpp`).

```
      +--------+   +--------+   +--------+   +--------+   +--------+   +--------+
  PC  |   IF   |   |   ID   |   |   OF   |   |   EX   |   |  MEM   |   |   WB   |
 ---> |        |-> |        |-> |        |-> |        |-> |        |-> |        |---> registers
      |mem[PC] |   | decode |   |read rs |   | ALU    |   | load   |   | reg    |     flags
 PC+1 |        |   |        |   |  rt    |   | branch |   | store  |   | write  |     SP
      +--------+   +--------+   +--------+   +--------+   +--------+   +--------+
                        ^            ^            |            |
                        |            |  forward <-+------------+
                        |            |  (EX/MEM and MEM/WB results, register file)
                        |            |
                        +------------+--- branch target, flushes IF, ID and OF
```

Latches between the stages: IF/ID, ID/OF, OF/EX, EX/MEM, MEM/WB.

## Stage by stage

| Stage | Reads | Writes |
|---|---|---|
| IF | `PC` | IF/ID: the fetched word and its address |
| ID | IF/ID | ID/OF: decoded signals |
| OF | ID/OF | OF/EX: the register operands |
| EX | OF/EX | EX/MEM: ALU result, flags, address, branch target, new SP |
| MEM | EX/MEM | EX/MEM: the loaded value; memory write for stores |
| WB | MEM/WB | the register file, memory, flags, SP, PC |

## Hazards

Same policy as the 4 stage pipeline, but the reading of operands moved to OF, so
the distance between a producer and its consumer is one stage longer:

- **Read after write (forwarding).** OF takes a value from the EX or MEM stage
  if the producer has already computed it, otherwise from the register file.
- **Load use (stall).** A load reads its data in MEM, so a dependent instruction
  waits one cycle in OF, then forwards from MEM.
- **Flags, stack pointer, memory order (stall).** Same rules as the 4 stage.
- **Control (flush).** A taken branch, jump, call or return clears IF, ID and OF,
  one stage more than the 4 stage pipeline, which is why `loop` and `recursion`
  cost noticeably more (see `docs/comparison_4_vs_6_stage.md`).

## Results

Cycles and hazard counts per program are in `docs/evaluation_matrix.md`. On this
program set the 6 stage pipeline has roughly double the CPI of the 4 stage one;
its benefit is a shorter path per stage (a higher clock), which a cycle count
cannot show. Every program still reaches the same state as the sequential
reference (`tests/test_pipeline_hazards.cpp`).
