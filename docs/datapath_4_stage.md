# 4 stage datapath

Implemented by `CyclePipeline` with `stages = 4` (see `simulator_core/cycle_pipeline.cpp`).

```
        +-------------+     +-------------+     +-------------+     +-------------+
   PC   |     IF      |     |     ID      |     |     EX      |     |     WB      |
  ----> |             | --> |             | --> |             | --> |             | ----> registers
        | read mem[PC]|     | decode      |     | ALU         |     | reg write   |       memory
  PC+1  |             |     | read rs, rt |     | address     |     | mem write   |       flags
        +-------------+     +-------------+     +-------------+     +-------------+       SP
              ^                   ^                 |                    |
              |                   |                 |                    |
              |                   |   forward  <----+--------------------+
              |                   |   (EX/WB result, and the register file)
              |                   |
              +-------------------+--- branch target, flushes IF and ID
```

Latches between the stages: IF/ID, ID/EX, EX/WB.

## Stage by stage

| Stage | Reads | Writes |
|---|---|---|
| IF | `PC` | IF/ID latch: the fetched word and its address |
| ID | IF/ID | ID/EX latch: decoded signals and the register operands |
| EX | ID/EX | EX/WB latch: ALU result, flags, address, branch target, new SP |
| WB | EX/WB | the register file, memory, flags, SP, PC |

## Hazards

- **Read after write (forwarding).** ID reads its operands, but a value written
  by the instruction in the EX or WB stage is taken from that stage directly
  instead of the register file, so ALU to ALU dependencies run without a stall.
- **Load use (stall).** A load's data is only read in WB, so the instruction
  behind it waits one cycle in ID. This is the `hazard_load_use` case in the
  evaluation table.
- **Flags (stall).** A branch reads the flags as committed, so it waits while a
  flag writing instruction is still in flight.
- **Stack pointer (stall).** Pushes, pops, calls and returns serialise on SP so
  the memory they touch is always in order.
- **Memory order (stall).** Two memory instructions never overlap.
- **Control (flush).** When EX resolves a taken branch, jump, call or return the
  instructions in IF and ID were fetched from the wrong path and are cleared;
  the target is fetched into IF.

## Results

Cycles and hazard counts per program are in `docs/evaluation_matrix.md`; the
comparison with the 6 stage pipeline is in `docs/comparison_4_vs_6_stage.md`.
Every program reaches the same state as the sequential reference
(`tests/test_pipeline_hazards.cpp`).
