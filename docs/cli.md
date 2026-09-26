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
| `step [count]` | run count instructions, one by default |
| `reset` | reset the CPU, the loaded program stays in memory |
| `regs` | show the registers, PC, SP and FLAGS |
| `mem [start] [count]` | show count words from start, 0x000 and 16 by default |
| `break <address\|label>` | stop before the instruction at the address |
| `delete <address\|label>` | remove a breakpoint |
| `breaks` | list the breakpoints |
| `watch <address\|label>` | stop when the address is written |
| `unwatch <address\|label>` | remove a watchpoint |
| `watches` | list the watchpoints |
| `help` | show the command list |
| `quit` | leave the simulator |

Addresses and counts are decimal, or hex with a `0x` prefix. Everywhere an address is
expected, a label from the symbol file is accepted too.

A breakpoint that sits inside a loop stops once per pass, so `run` has to be repeated to
walk the loop.

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
would drop below 0xC00. PC is left on the instruction that faulted and nothing was
written back.

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
