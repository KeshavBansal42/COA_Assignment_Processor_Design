# Arithmetic Logic Unit (ALU)

The ALU performs the processor's integer, logical and binary32 floating-point
operations. Addition uses a hierarchical carry-lookahead adder (CLA),
multiplication uses a carry-save Wallace tree, integer division uses restoring
division, and floating-point division uses Newton–Raphson with an exact final
correction.

## Files and connection to the processor

| File | Responsibility |
|---|---|
| `alu/alu.hpp` | ALU interface and declarations of shared arithmetic helpers |
| `alu/alu.cpp` | Integer operations, CLA, carry-save and Wallace multiplication, restoring division, Newton reciprocal, and operation dispatch |
| `alu/float32.hpp` | Binary32 API, rounding modes and floating-point status |
| `alu/float32.cpp` | Unpacking, special values, alignment, normalization, rounding and packing for floating-point arithmetic |
| `tests/test_alu.cpp` | Integer operations and shared arithmetic helpers |
| `tests/test_float32.cpp` | Binary32 results, rounding modes and status |
| `tests/test_float.cpp` | Floating-point instructions through the processor |

`Alu::execute(AluOp, uint32_t, uint32_t)` takes an operation and two raw 32-bit
words. It returns an `AluResult` containing the result word, candidate Z/N/C/V
flags and an exception code. The ALU itself does not modify registers, memory
or CPU FLAGS; the pipeline applies the control signals and commits the result.

Registers have no integer or floating-point type tag. The decoded instruction
selects the interpretation: ADD treats its operands as integer words, while
FADD treats the same bits as binary32 encodings. For example, `0x3F800000`
represents 1.0 to FADD but the integer 1065353216 to ADD. The ALU does not
inspect a value and guess its type, and MOV, loads and stores preserve its bits.
Floating-point operations share the integer arithmetic helpers directly;
they do not call the public integer instructions or convert through host floats.

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
rejects that result before committing the faulting instruction's writes. Integer
operations otherwise return NONE.

## Arithmetic algorithms

### Hierarchical carry-lookahead addition

`detail::claAdd` computes bit generate/propagate signals, combines them into
groups of 2/4/8/16/32 bits, distributes carries back down the tree, and uses
sixteen 2-bit sum blocks to produce the result. SUB/CMP reuse it as a + ~b + 1.
Signed overflow is the XOR of the carry into and out of bit 31. The tree phases are not simulated cycles.

### Carry-save and Wallace multiplication

Integer MUL and floating FMUL use a carry-save Wallace tree. The helper
`detail::boothMultiply` remains a tested internal reference, not an alternative runtime path or a new instruction. It initializes
A=0, Q=b, M=a and Q_minus_1=0. On each of 32 iterations, the pair
(Q bit 0, Q_minus_1) determines the action: 01 adds M, 10 subtracts M, and
00/11 leave A unchanged. Each iteration then performs an arithmetic right shift
of the combined A, Q and Q_minus_1 registers.

A and the sign-extended M are 33 bits so that negating INT32_MIN is
representable. Each addition/subtraction uses shared `addSubtract33`: the
existing 32-bit CLA plus the extra high-bit equation. `detail::boothMultiply`
retains the complete 64-bit product, discarding only the redundant guard sign
bit. Both multipliers retain all 64 product bits internally. Integer MUL selects
the Wallace result's low word and checks that its high word extends the low
word's sign. Thus INT32_MIN * 1 clears V and INT32_MIN * -1 sets V.

`detail::carrySave` compresses three 64-bit rows to two using only Boolean logic:

```text
sum   = a XOR b XOR c
carry = ((a AND b) OR (a AND c) OR (b AND c)) << 1
```

The shifted carry is already part of the returned row; sum+carry equals a+b+c
modulo 2^64. No carry propagates between columns within a compressor.
`wallaceMultiply` generates 32 shifted partial-product rows using multiplier
bits. Each level reduces independent triples from the previous level, so the
row counts are 32,22,15,10,7,5,4,3,2: eight carry-save layers. A final 64-bit
addition combines the last two rows. Both `add64` and `subtract64` use two
32-bit CLA blocks, passing the low block's carry/no-borrow to the high block.

The unsigned primitive returns the full 32x32 product, including bit 63.
`signedWallaceMultiply` converts it to the two's-complement signed product by
CLA-subtracting (b<<32) when a is negative and (a<<32) when b is negative.
These are the sign corrections from expanding (a-sa*2^32)(b-sb*2^32), modulo
2^64, where sa and sb are the sign bits of a and b. FMUL and Newton use the
unsigned primitive directly; their unsigned fixed-point operands must not go
through the public integer MUL instruction.

### Unsigned restoring division

`detail::restoringDivide` initializes A=0, Q=dividend and M=divisor. It
performs 32 iterations, shifting A/Q left, subtracting M, testing the trial
remainder, and restoring A if negative. A failed trial
leaves the new quotient bit zero; a successful trial sets it to one.

A uint64 container holds the 33-bit partial remainder/trial word. Shared
`detail::restoringStep` subtracts through the CLA extension, tests bit 32 as the
trial sign and adds the divisor back through the same extension if negative.
This safely handles the full unsigned 32-bit operand range. On success the
internal result retains quotient Q and remainder A, satisfying
dividend = quotient * divisor + remainder and 0 <= remainder < divisor.
The internal helper declared in `alu/alu.hpp` allows direct verification;
the public ALU and processor DIV still expose only the quotient. On division
by zero the internal helper returns zero quotient/remainder and DIV_BY_ZERO.
Integer DIV uses this CLA-backed restoring step. FDIV uses Newton–Raphson,
described below.

### Shared integer and floating-point datapath

The shared helpers are declared in `alu/alu.hpp` and implemented in
`alu/alu.cpp` under `risc201::detail`. They compute values without modifying
CPU FLAGS or FP status.

| Primitive | Users |
|---|---|
| claAdd / add32 / subtract32 | integer ADD/SUB/CMP, FP significands, rounding increment |
| add64 / subtract64 | Wallace final addition/sign corrections, Newton exact residual correction |
| addSigned32 / subtractSigned32 | FP exponent unbiasing, alignment, normalization, rebiasing |
| addSubtract33 | Booth accumulator and restoring divider trial/restoration |
| carrySave / wallaceMultiply / signedWallaceMultiply | active integer MUL, FP FMUL, Newton products |
| boothMultiply | baseline full-product reference in arithmetic tests |
| restoringStep | integer DIV quotient bits |
| newtonReciprocal / newtonDivideSignificands | FDIV reciprocal estimate and exact quotient/remainder |

The arithmetic is expressed through shared datapath algorithms. Shifts, Boolean
operations, comparisons, field extraction and loop indices use ordinary C++.
The simulator models the arithmetic structure, but does not schedule access to
a single physical adder or simulate every operation at gate level.

### Newton-Raphson floating-point division

FDIV uses the reciprocal iteration x <- x*(2-d*x), beginning at
x=1/2. It performs the iteration in fixed point, using Wallace products and
CLA subtraction, rather than repeatedly rounding through binary32 FMUL.
All 64 product bits are retained before each fixed-point shift.

Let S=2^31. For a normalized 24-bit divisor b, D=b<<8 represents d=D/S in
[1,2). The reciprocal x is Q1.31, and starts at S/2. Six iterations compute:

```text
t      = floor(D*x / S)
factor = 2*S - t
x      = floor(x*factor / S)
```

The correction factor fits in uint32. Since 2*S=2^32, `subtract32(0,t)` computes
it exactly modulo 2^32. Loop counters use host arithmetic; divisor, reciprocal,
factor, products, exponent arithmetic and residual arithmetic use the shared
datapath primitives. No host floating arithmetic, native operand multiplication
or division, or reciprocal lookup table is used.

The numerator n is normalized so b <= n < 2*b (possibly by a left shift and
exponent adjustment). The initial 27-bit quotient estimate is floor(n*x/2^28).
The module computes its exact product q*b with Wallace and compares it with
T=n<<26. If the product exceeds T, it decrements q once. If T-q*b >= b, it
increments q once. CLA arithmetic then returns the exact remainder T-q*b.
There is no restoring-divider fallback or unbounded correction loop. A nonzero
remainder supplies sticky to the shared round/pack stage, preserving all
four rounding modes and gradual underflow.

#### Why a single quotient correction is sufficient

The exact fixed-point reciprocal is x*=S/d. An iteration before its final
truncation equals x* - (d/S)*(x-x*)^2 plus the inner truncation contribution,
which is less than x/S. Inductively 0 < x < x*+2 <= S+2; the combined inner
and outer truncation error has magnitude less than 2 integer units. For the
relative error e=1-d*x/S this gives |e_next| <= |e|^2 + 4/S, with |e_0|<=1/2.
The conservative recurrence u_0=1/2, u_next=u^2+4/S gives u_6<8/S. Consequently
|x-x*|<8. Since n<2^25, the untruncated quotient estimate has absolute error
less than n*8/2^28<1. Its floor can differ from floor(T/b) by at most one.
The exact product/residual check therefore recovers the correct quotient and
remainder with at most one increment or decrement. Debug assertions also check
the normalized-input preconditions and the corrected remainder bound.

The FP tests check all rounding modes after this exact correction; the integer
tests verify reciprocal error bounds and both correction directions directly.
There are six fixed iterations (twelve Wallace products), one numerator/reciprocal
product, and one exact quotient/divisor product. This models the algorithm, not
a measured CPU speedup. Existing CPU cycle counts remain unchanged.

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

Logic instructions write the entire Flags structure, so they clear C/V as
well as updating Z/N. This behavior is shared by both control encodings.

## Timing and floating point

Each ALU call completes within one EX invocation. Arithmetic iterations and
CSA layers do not advance simulated cycles.
Multicycle execution needs coordination with pipeline stalls and forwarding.

FADD/FSUB/FMUL/FDIV use the explicit binary32 module described below. They
preserve architectural FLAGS and complete in one EX invocation too.

## Binary32 floating point

`alu/float32.hpp` exposes stateless `fp32::add`, `subtract`, `multiply`, and
`divide`. Operands and results are uint32 words: sign bit 31, exponent bits
30-23 (bias 127), fraction bits 22-0. There is no native float/double arithmetic
or dependence on the host rounding environment in the implementation.

Finite nonzero inputs are unpacked into an unbiased exponent and a normalized
24-bit significand. Subnormals use exponent -126 and no implicit leading one;
they are normalized internally without losing information. The arithmetic
path handles alignment, significand arithmetic, normalization, rounding and
packing as follows:

- ADD/SUB align exponents with a right shift that retains discarded bits in a
  sticky bit. Same signs add magnitudes; different signs subtract magnitudes.
  The 27-bit extended significands and 28-bit sum fit in the shared 32-bit CLA.
  The result is normalized before rounding.
- MUL uses the shared unsigned Wallace tree for the exact 48-bit significand
  product, CLA-adds unbiased exponents, and XORs signs.
- DIV CLA-subtracts unbiased exponents and uses the Newton reciprocal iteration
  plus exact correction described above to obtain 27 quotient bits and remainder.
  A nonzero remainder contributes sticky. Integer DIV remains restoring.
- A shared round/pack stage keeps guard, round and sticky information, supports
  gradual underflow, rounds once using a CLA-based increment, and handles a carry
  from rounding. Exponent changes throughout the FP path also use the CLA.

### Rounding modes

| API mode | Rule |
|---|---|
| NearestEven (default) | nearest representable value; exact ties choose an even retained LSB |
| TowardZero | discard excess magnitude |
| TowardPositive | round toward positive infinity |
| TowardNegative | round toward negative infinity |

CPU instructions always select NearestEven. The other modes are selectable by
callers of the fp32 module; no CPU rounding register or new instruction is added.
For example, 1 + 2^-24 returns 1 under NearestEven and the next larger binary32
value under TowardPositive. The host rounding mode does not change either result.

### Special values and status

Every operation returns `fp32::Result { value, status }`. Status is per call,
not sticky or global, and contains invalid, divideByZero, overflow, underflow,
and inexact booleans. It is separate from integer Z/N/C/V and ExceptionCode.

| Case | Value/status |
|---|---|
| Quiet NaN input | canonical quiet NaN 0x7FC00000 |
| Signaling NaN input | canonical quiet NaN, invalid |
| Opposing infinities in ADD, equal infinities in SUB, 0*infinity, 0/0, infinity/infinity | canonical quiet NaN, invalid |
| Finite nonzero / signed zero | signed infinity, divideByZero |
| Infinity / zero | signed infinity; no divideByZero |
| Finite / infinity | signed zero |
| Overflow | overflow + inexact; signed infinity or maximum finite value, depending on rounding direction |
| Tiny, inexact rounded result | underflow + inexact |
| Exactly representable subnormal | exact result, no underflow |

NaN inputs are handled before the other special-value cases. Either signaling
NaN sets invalid, including when the other operand is a quiet NaN. NaN signs
and payloads are canonicalized.
Tininess is detected **after rounding**: a tiny value rounded up to minimum
normal reports inexact but not underflow. Exact cancellation produces -0 only
under TowardNegative; like-signed zeros retain their sign. MUL/DIV zero signs
are the XOR of operand signs.

The CPU's AluResult retains its original interface. Its floating dispatch uses
the module's value and does not surface FP status or raise integer exceptions.
FP status is available to direct module callers/tests. Connecting it to a CPU
status register is a separate team integration decision. Candidate Z/N flags
returned by Alu::execute still describe raw result bits and are not FP comparisons;
the control unit does not commit them for floating instructions. For example,
the raw word for -0 has N=true and Z=false in the returned candidate flags,
even though its floating-point value is zero.

### Scope and limitations

The implementation covers four binary32 arithmetic operations and the four
rounding modes listed above. It does not implement binary64, floating-point
comparison or conversion instructions, fused multiply-add, or a CPU floating-point
status/rounding register. Integer DIV is unsigned and exposes no remainder
instruction. Goldschmidt and non-restoring division are not implemented.
Booth is retained for reference tests; there is no instruction or runtime
setting that switches between multiplication algorithms.

The simulator has no hardware area or clock-frequency model. No measured
speedup is claimed for the Wallace tree or Newton iteration.

## Verification

`tests/test_alu.cpp` checks directed examples, all pairs of signed/unsigned
boundary values, carry/borrow propagation across each bit boundary, signed
comparison flags, multiplication symmetry, quotient/remainder invariants,
logic flags, ignored unary operands, and shift-count masking. Deterministic
random inputs include both full-width and small divisors. Reference arithmetic
uses wider host integers independently of the implemented algorithms.
The suite additionally checks all 64 product bits, 33-bit arithmetic boundaries,
signed exponent arithmetic, restoring-step branches, and maximum FP significands.
It also verifies carry-save conservation, signed and unsigned full Wallace
products against native integer arithmetic and Booth, 64-bit CLA carry/borrow,
reciprocal error bounds, and 8,221 exact Newton quotient/remainder cases.

The checks in `test_alu` and `test_float32` remain enabled under `NDEBUG`.
The integer suite can also be compiled directly:

```sh
g++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=undefined,address -I . \
    alu/alu.cpp alu/float32.cpp tests/test_alu.cpp -o /tmp/risc201-test-alu
/tmp/risc201-test-alu
```

`tests/test_float32.cpp` checks directed halfway cases, all rounding modes,
special values, gradual underflow, rounding into minimum normal, and status
independence from host fenv. It also compares 503,056 results/status records
against host binary32 arithmetic using strict rounding compiler options. NaNs
are canonicalized for comparison; host underflow for a rounded normal result
is suppressed to allow hosts that detect tininess before rounding. Directed
tests independently enforce the module's after-rounding underflow policy.
Directed division cases include recurring fractions and reciprocals immediately
above/below one, exercising Newton precision and final rounding boundaries.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -frounding-math -fno-fast-math \
    -ffp-contract=off -I . alu/alu.cpp alu/float32.cpp tests/test_float32.cpp \
    -o /tmp/risc201-test-float32
/tmp/risc201-test-float32
```

`tests/test_float.cpp` checks instruction dependencies, special values and
preservation of integer FLAGS across all six combinations of sequential,
four-stage and six-stage execution with horizontal and vertical control.
The wider project suite checks control, pipelines, exceptions, memory,
assembler/disassembler and simulator interfaces.

To build and run the full suite from the project root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

For just the arithmetic tests, build targets `test_alu` and `test_float32`,
then run `ctest --test-dir build -R '^(alu|float32)$' --output-on-failure`.
Debug builds also keep assertions enabled in the rest of the project.

The directed and deterministic random tests provide substantial coverage, but
they are not exhaustive over every operand pair. The floating-point oracle is
host binary32 arithmetic. Passing these tests does not constitute formal
IEEE 754 conformance certification.
