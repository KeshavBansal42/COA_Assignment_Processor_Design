# Simulator CLI

`risc201-sim` is the command line shell for the simulator. It reads commands from standard
input, so it can be driven by hand or by a pipe.

    cmake -S . -B build
    cmake --build build
    ./build/risc201-sim

Passing a program on the command line loads it before the first prompt:

    ./build/risc201-sim tests/programs/loop.hex

## Program files

A program file holds one 32-bit word per line in hex, for example
`tests/programs/stack_fault.hex`:

    ; risc201 demo - deliberate stack fault

    44400001  ; LDI  r1, 1
    97ffffff  ; CALL -1

- Words are hex, with or without a `0x` prefix, and can be separated by spaces.
- `#` or `;` starts a comment, to the end of the line.
- A program longer than the 0x800 word code region is rejected.

This is the shape the assembler's hex output takes, so its output can be loaded directly.

## Symbol files

`symbols <file>` loads labels so they can be used wherever an address is expected. Each
line is `name address` or `name = address`, and comments work the same way:

    ; labels
    start 0
    loop = 3

## Commands

| Command | What it does |
|---|---|
| `load <file>` | load a program, clearing the machine first |
| `symbols <file>` | load labels |
| `run` | run until a breakpoint, a watchpoint, HALT or an exception |
| `continue` | the same as `run` |
| `step [count]` | run count steps, one by default (an instruction in sequential mode, a clock cycle in the overlapped modes) |
| `reset` | reset the CPU, the loaded program stays in memory |
| `regs` | show the registers, PC, SP and FLAGS |
| `mem [start] [count]` | show count words from start, 0x000 and 16 by default |
| `break <address\|label>` | stop before the instruction at the address |
| `delete <address\|label>` | remove a breakpoint |
| `breaks` | list the breakpoints |
| `watch <address\|label>` | stop when the address is written |
| `unwatch <address\|label>` | remove a watchpoint |
| `watches` | list the watchpoints |
| `stack [n]` | render the top n stack words as an ASCII stack frame |
| `dynamic <on\|off>` | let the stack grow into the data region, or restore the fixed `0xC00` floor |
| `mode <name>` | pick `sequential`, `4stage` or `6stage` (restarts the program) |
| `micro <name>` | pick `horizontal` or `vertical` control memory |
| `microtrace [address]` | show the microPC stepping through an instruction |
| `predictor <dynamic|static>` | select 2-bit prediction or always-not-taken baseline and restart |
| `cycles` | show cycles, instructions, hazards and conditional branch prediction counts |
| `help` | show the command list |
| `quit` | leave the simulator |

Addresses and counts are decimal, or hex with a `0x` prefix. Everywhere an address is
expected, a label from the symbol file is accepted too.

A breakpoint that sits inside a loop stops once per pass, so `run` has to be repeated to
walk the loop.

## Pipelines

`mode` selects the executor. `sequential` runs one whole instruction per `step` (the
reference model). `4stage` and `6stage` advance one clock cycle per `step`, with
forwarding, hazard stalls and branch flushes, so an instruction takes several steps to
retire. `cycles` reports the cycle and instruction totals plus the stall, flush and
forward counters.

Breakpoints stop before the next fetch. In the overlapped modes that is the fetch
stage's address (`nextPc()`), which runs ahead of the last committed write back.

## Microcode

`micro horizontal` reads one 32-bit control word per instruction; `micro vertical`
walks a short 16-bit microprogram through a sequencer. Both produce the same control
signals, so the program result does not change.

`microtrace` shows the control store walk for the instruction at the current PC, or at
the address given:

    > micro vertical
    control mode vertical
    > microtrace 3
    microtrace PC 0x00000003  word 0x08cc4000  ADD [vertical]
      microPC 0x0003  word 0x00000001  ALU_REG alu=ADD
      microPC 0x0004  word 0x00000003  REG_WRITE
      microPC 0x0005  word 0x00000004  FLAGS
      microPC 0x0006  word 0x00000000  END  (sequencer stop)
      decoded signals: regWrite writeFlags  aluSrc=register  resultSrc=alu

An undefined opcode is reported by the trace as `ILLEGAL_INSTRUCTION`, which is the
same exception the pipeline raises when it tries to execute it.

## Stack view

`stack` prints the full-descending stack as ASCII frames, with the SP marker, hex and
decimal values, and a frame boundary guessed from a saved return address:

    > stack 4
    +----------------- STACK -----------------+
    | 0x0FFF | 0x00001111 |       4369 |
    | 0x0FFE | 0x00002222 |       8738 |
    ...
    +-----------------------------------------+

By default the stack floor is fixed at `0xC00`. `dynamic on` moves the floor to the
heap high-water mark (`0x800` while the heap is empty), so the stack can grow through
the data region; `dynamic off` restores the fixed floor. The `stack` view renders down
to the current floor, and the red overflow warning follows it too.

    > dynamic on
    dynamic stack on  stack floor 0x0800
    > dynamic off
    dynamic stack off  stack floor 0x0C00

## A demo run

    > load tests/programs/loop.hex
    loaded 8 words
    > run
    halted at PC 0x00000007
    > regs
    r0 = 0x00000000  r1 = 0x00000000  r2 = 0x00000000  r3 = 0x00000037
    ...
    PC = 0x00000007  SP = 0x00001000
    FLAGS Z=1 N=0 C=1 V=0

The loop program adds 10 down to 1, so r3 ends up holding 55, which is 0x37.

The stack fault program shows the exception path:

    > load tests/programs/stack_fault.hex
    loaded 2 words
    > run
    exception STACK_OVERFLOW at PC 0x00000001
    > regs
    ...
    PC = 0x00000001  SP = 0x00000c00

The CALL at 0x001 calls itself, so each pass pushes another return address until SP
would drop below the stack floor: 0xC00 in the default fixed mode (the run above), or
the heap top after `dynamic on` (the same program then stops at SP 0x800). PC is left
on the instruction that faulted and nothing was written back.

## A debug session

A breakpoint stops before its instruction runs:

    > load tests/programs/loop.hex
    loaded 8 words
    > break 3
    breakpoint at 0x00000003
    > run
    breakpoint at 0x00000003
    > regs
    r0 = 0x00000000  r1 = 0x0000000a  r2 = 0x00000000  r3 = 0x00000000
    ...
    > step
    stopped at PC 0x00000004

The ADDI at 0x003 has not run yet, so r1 still holds the counter and r3 is empty. After
`step`, the ADD has run and PC has moved on.

A watchpoint stops after the write. The recursion program pushes its first return address
onto the stack at 0xFFF, so watching that address catches the CALL:

    > load tests/programs/recursion.hex
    loaded 14 words
    > watch 0xfff
    watchpoint at 0x00000fff
    > run
    watchpoint at 0x00000fff now 0x00000003

## Conditional branch prediction

The overlapped pipelines default to a 64-entry PC-indexed table of two-bit saturating
counters, initialized weakly not taken. `predictor static` selects always not taken;
`predictor dynamic` selects the table. Either command resets the machine and predictor
history, while keeping the loaded program. `cycles` reports resolved conditional branches
and direction mispredictions. Sequential mode does not perform speculative prediction.
