# ALU implementation and arithmetic contract

`Alu::execute(AluOp, uint32_t, uint32_t)` returns a value, candidate Z/N/C/V
flags, and an exception. It does not modify CPU state. This document records
the existing instruction behavior; it introduces no opcodes or flag-policy changes.

## Integer operations

All values are 32-bit words. ADD/SUB wrap modulo 2^32; MUL returns the low
32 product bits. Signed arithmetic interprets words as two's complement.
Overflow sets V where specified, rather than raising an exception.

| Operation | Result | Returned C | Returned V |
|---|---|---|---|
| ADD | a + b | unsigned carry out | signed addition overflow |
| SUB, CMP | a - b | no borrow: a >= b as unsigned words | signed subtraction overflow |
| MUL | low 32 bits of signed a * b | false | full signed product does not fit in int32 |
| DIV | unsigned a / b, truncated toward zero | false | false |
| AND, OR, XOR | bitwise operation on a and b | false | false |
| NOT | complement of a; b ignored | false | false |
| MOV | a; b ignored | false | false |
| SHL | a shifted left by (b & 31) | false | false |
| SHR | logical, zero-filled right shift by (b & 31) | false | false |

Every operation returns Z = (result == 0) and N = result bit 31. These are
candidate flags even for instructions that do not commit FLAGS. Shift counts
are modulo 32: counts 0 and 32 both return a unchanged. There is no arithmetic
right-shift instruction. DIV interprets even a word with bit 31 set as unsigned;
for example, 0xFFFFFFF6 / 2 = 0x7FFFFFFB, not -5.

DIV by zero returns value 0, Z=true, N=C=V=false, and DIV_BY_ZERO. The pipeline
must reject that result before writing any architectural state. Integer
operations otherwise return NONE.

## Algorithms and relationship to the arithmetic slides

### Hierarchical carry-lookahead addition

`cla_add` implements Arithmetic-1's slides 16-25: bit generate/propagate,
bottom-up groups of 2/4/8/16/32 bits, top-down carry distribution, then sixteen
2-bit sum blocks. SUB/CMP reuse it as a + ~b + 1. Signed overflow is the XOR
of the carry into and out of bit 31. The tree phases are not simulated cycles.

### Signed Booth multiplication

`booth_mul` follows Arithmetic-1's slides 30-31. Initialize A=0, Q=b, M=a,
Q_minus_1=0. Repeat 32 times: pair 01 adds M, pair 10 subtracts M, pairs
00/11 do no arithmetic, then arithmetic-right-shift the combined registers.

A and the sign-extended M are 33 bits so that negating INT32_MIN is
representable. Each addition/subtraction uses the existing 32-bit CLA plus
the extra high-bit equation. After the iterations, all 33 bits of A must
equal the sign extension of Q for the low result to fit in signed 32 bits.
Thus INT32_MIN * 1 clears V and INT32_MIN * -1 sets V.

### Unsigned restoring division

`detail::restoringDivide` follows Arithmetic-1's slide 34: initialize A=0,
Q=dividend, M=divisor; repeat 32 times, shifting A/Q left, subtracting M,
testing the trial remainder, and restoring A if negative. A failed trial
leaves the new quotient bit zero; a successful trial sets it to one.

An int64 accumulator safely holds the shifted partial remainder and negative
trial values across the full unsigned 32-bit operand range. On success the
internal result retains quotient Q and remainder A, satisfying
dividend = quotient * divisor + remainder and 0 <= remainder < divisor.
The internal helper in `alu/integer_division.hpp` allows direct verification;
the public ALU and processor DIV still expose only the quotient. On division
by zero the internal helper returns zero quotient/remainder and DIV_BY_ZERO.
The divider uses host integer subtraction/addition to model its arithmetic;
it does not currently reuse the CLA.

## Architectural flag commitment

Both control encodings select the same flag policy. The pipeline commits the
entire candidate Flags structure only when ControlSignals::writeFlags is true.

| Instructions | CPU FLAGS behavior |
|---|---|
| ADD, SUB, MUL, CMP, ADDI | commit Z/N/C/V |
| AND, OR, XOR, NOT | commit Z/N and clear C/V |
| DIV, SHL, SHR, MOV, LDI, LW, SW, floating-point instructions | preserve FLAGS |

CMP does not write a destination register. BEQ/BNE consume Z; BLT/BGE use
N != V / N == V for signed comparisons. ADDI sign-extends its 18-bit immediate
before addition. LDI uses ADD with operand a=0. LW/SW use ADD for addresses,
then memory safety checks the resulting address.

The older ISA wording "sets Z N" for logic instructions did not specify C/V.
Clearing C/V above documents the existing implementation, not a newly agreed
policy. Preserving C/V instead would require team agreement and coordinated
control/pipeline changes; it must not be changed inside the ALU alone.

## Timing and floating point

The ALU remains stateless and synchronous: every execute call completes in
one EX invocation. Booth/division iterations do not advance simulated cycles.
Multicycle execution needs coordination with pipeline stalls and forwarding.

FADD/FSUB/FMUL/FDIV still use native IEEE binary32 host arithmetic and raw
register bit patterns. They preserve architectural FLAGS; FDIV by zero follows
floating-point infinity/NaN rules. Explicit floating-point algorithms,
rounding controls, and floating-point status reporting are separate future work.

## Verification

`tests/test_alu.cpp` checks directed examples, all pairs of signed/unsigned
boundary values, carry/borrow propagation across each bit boundary, signed
comparison flags, multiplication symmetry, quotient/remainder invariants,
logic flags, ignored unary operands, and shift-count masking. Deterministic
random inputs include both full-width and small divisors. Reference arithmetic
uses wider host integers independently of the implemented algorithms.

All checks remain enabled under NDEBUG. Run the ALU
directly if another module prevents building the full core:

```sh
g++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=undefined,address -I . \
    alu/alu.cpp tests/test_alu.cpp -o /tmp/risc201-test-alu
/tmp/risc201-test-alu
```
