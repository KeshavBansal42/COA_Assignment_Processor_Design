# Pipeline and microcode evaluation

Runs every demo and hazard program under each pipeline and each control
encoding. Cycles and instructions are per run, CPI is cycles per retired
instruction, and the control columns are how many control memory words
and bits the run reads (one 32-bit word per instruction for horizontal,
the microprogram length in 16-bit words for vertical).

| Program | Pipeline | Microcode | Instructions | Cycles | CPI | Stalls | Flushes | Forwards | Control words | Control bits |
|---|---|---|---|---|---|---|---|---|---|---|
| loop.hex | sequential | horizontal | 44 | 44 | 1.00 | 0 | 0 | 0 | 8 | 256 |
| loop.hex | sequential | vertical | 44 | 44 | 1.00 | 0 | 0 | 0 | 24 | 384 |
| loop.hex | 4stage | horizontal | 43 | 65 | 1.51 | 10 | 9 | 11 | 8 | 256 |
| loop.hex | 4stage | vertical | 43 | 65 | 1.51 | 10 | 9 | 11 | 24 | 384 |
| loop.hex | 6stage | horizontal | 43 | 95 | 2.21 | 20 | 18 | 11 | 8 | 256 |
| loop.hex | 6stage | vertical | 43 | 95 | 2.21 | 20 | 18 | 11 | 24 | 384 |
| recursion.hex | sequential | horizontal | 40 | 40 | 1.00 | 0 | 0 | 0 | 14 | 448 |
| recursion.hex | sequential | vertical | 40 | 40 | 1.00 | 0 | 0 | 0 | 38 | 608 |
| recursion.hex | 4stage | horizontal | 39 | 62 | 1.59 | 9 | 11 | 0 | 14 | 448 |
| recursion.hex | 4stage | vertical | 39 | 62 | 1.59 | 9 | 11 | 0 | 38 | 608 |
| recursion.hex | 6stage | horizontal | 39 | 94 | 2.41 | 18 | 22 | 4 | 14 | 448 |
| recursion.hex | 6stage | vertical | 39 | 94 | 2.41 | 18 | 22 | 4 | 38 | 608 |
| stack_fault.hex | sequential | horizontal | 1026 | 1026 | 1.00 | 0 | 0 | 0 | 2 | 64 |
| stack_fault.hex | sequential | vertical | 1026 | 1026 | 1.00 | 0 | 0 | 0 | 6 | 96 |
| stack_fault.hex | 4stage | horizontal | 1025 | 2052 | 2.00 | 0 | 1024 | 0 | 2 | 64 |
| stack_fault.hex | 4stage | vertical | 1025 | 2052 | 2.00 | 0 | 1024 | 0 | 6 | 96 |
| stack_fault.hex | 6stage | horizontal | 1025 | 4101 | 4.00 | 0 | 2048 | 0 | 2 | 64 |
| stack_fault.hex | 6stage | vertical | 1025 | 4101 | 4.00 | 0 | 2048 | 0 | 6 | 96 |
| hazard_raw.hex | sequential | horizontal | 7 | 7 | 1.00 | 0 | 0 | 0 | 7 | 224 |
| hazard_raw.hex | sequential | vertical | 7 | 7 | 1.00 | 0 | 0 | 0 | 24 | 384 |
| hazard_raw.hex | 4stage | horizontal | 6 | 9 | 1.50 | 0 | 0 | 4 | 7 | 224 |
| hazard_raw.hex | 4stage | vertical | 6 | 9 | 1.50 | 0 | 0 | 4 | 24 | 384 |
| hazard_raw.hex | 6stage | horizontal | 6 | 11 | 1.83 | 0 | 0 | 7 | 7 | 224 |
| hazard_raw.hex | 6stage | vertical | 6 | 11 | 1.83 | 0 | 0 | 7 | 24 | 384 |
| hazard_load_use.hex | sequential | horizontal | 6 | 6 | 1.00 | 0 | 0 | 0 | 6 | 192 |
| hazard_load_use.hex | sequential | vertical | 6 | 6 | 1.00 | 0 | 0 | 0 | 18 | 288 |
| hazard_load_use.hex | 4stage | horizontal | 5 | 10 | 2.00 | 2 | 0 | 1 | 6 | 192 |
| hazard_load_use.hex | 4stage | vertical | 5 | 10 | 2.00 | 2 | 0 | 1 | 18 | 288 |
| hazard_load_use.hex | 6stage | horizontal | 5 | 13 | 2.60 | 3 | 0 | 3 | 6 | 192 |
| hazard_load_use.hex | 6stage | vertical | 5 | 13 | 2.60 | 3 | 0 | 3 | 18 | 288 |
| hazard_branch.hex | sequential | horizontal | 6 | 6 | 1.00 | 0 | 0 | 0 | 7 | 224 |
| hazard_branch.hex | sequential | vertical | 6 | 6 | 1.00 | 0 | 0 | 0 | 19 | 304 |
| hazard_branch.hex | 4stage | horizontal | 5 | 10 | 2.00 | 1 | 1 | 1 | 7 | 224 |
| hazard_branch.hex | 4stage | vertical | 5 | 10 | 2.00 | 1 | 1 | 1 | 19 | 304 |
| hazard_branch.hex | 6stage | horizontal | 5 | 15 | 3.00 | 2 | 2 | 2 | 7 | 224 |
| hazard_branch.hex | 6stage | vertical | 5 | 15 | 3.00 | 2 | 2 | 2 | 19 | 304 |

