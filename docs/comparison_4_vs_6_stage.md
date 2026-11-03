# 4 stage vs 6 stage pipeline

Both pipelines are overlapped and cycle accurate, and both reach exactly the
same machine state as the sequential reference (`tests/test_pipeline_hazards.cpp`).
They differ in how many stages an instruction passes through and therefore in
how many cycles a dependency or a branch costs.

- **4 stage:** IF, ID (decode and read operands), EX (ALU, branch resolve,
  address), WB (register, memory, flags, SP).
- **6 stage:** IF, ID (decode), OF (read operands), EX (ALU, branch resolve),
  MEM (load, store), WB (register, flags, SP).

## Cycles per program

From `docs/evaluation_matrix.md` (horizontal microcode rows):

| Program | Sequential | 4 stage | 6 stage |
|---|---|---|---|
| loop | 44 (1.00 CPI) | 65 (1.48) | 95 (2.16) |
| recursion | 40 (1.00) | 62 (1.55) | 94 (2.35) |
| stack_fault | 1026 (1.00) | 2052 (2.00) | 4101 (4.00) |
| stack_macros | 12 (1.00) | 21 (1.75) | 30 (2.50) |
| hazard_raw | 7 (1.00) | 9 (1.29) | 11 (1.57) |
| hazard_load_use | 6 (1.00) | 10 (1.67) | 13 (2.17) |
| hazard_branch | 6 (1.00) | 10 (1.67) | 15 (2.50) |

## Where the extra cycles go

| Program | 4 stage stalls/flushes | 6 stage stalls/flushes |
|---|---|---|
| loop | 10 / 9 | 20 / 18 |
| recursion | 9 / 11 | 18 / 22 |
| stack_fault | 0 / 1024 | 0 / 2048 |
| stack_macros | 7 / 0 | 14 / 0 |
| hazard_raw | 0 / 0 (4 forwards) | 0 / 0 (7 forwards) |
| hazard_load_use | 2 / 0 | 3 / 0 |
| hazard_branch | 1 / 1 | 2 / 2 |

- **Branches.** A taken branch flushes the instructions fetched behind it and
  refetches from the target. In the 6 stage pipeline there are three stages
  behind EX (IF, ID, OF) instead of two, so every taken branch wastes more work.
  This is why `loop`, `recursion` and `stack_fault` cost so much more.
- **Read after write.** Register forwarding removes the stall when the producer
  has reached EX, which is why `hazard_raw` has zero stalls and only pays the
  pipeline fill and drain. `hazard_load_use` still needs one bubble for the load
  use, and the deeper pipeline puts a few more instructions between the load and
  its use, so the 6 stage pays slightly more.
- **Stack pointer.** Pushes and calls are serialised on SP, so `stack_fault`,
  which calls itself 1024 times, costs two cycles per call in the 4 stage and
  four in the 6 stage.

## Honest reading

For this ISA and these programs the 6 stage pipeline is **worse**, not better:
CPI roughly doubles. The extra stages buy a shorter critical path per stage,
which in real silicon means a **higher clock frequency**; a cycle-count model
cannot show that, so all it sees is the larger fill, drain and flush cost. A
fair evaluation would multiply cycles by a per-stage clock period, and the 6
stage would only win when the shorter period outweighs the extra cycles.

The forwarding, stall and flush logic is shared between the two configurations,
so the comparison is between stage counts, not between two different machines.
