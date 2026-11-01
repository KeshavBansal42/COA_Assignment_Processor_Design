#include "alu/alu.hpp"
#include "alu/float32.hpp"
#include <cstring>

namespace risc201 {

// ---------------------------------------------------------------------------
// 32-bit Carry Lookahead Adder (CLA)
// Implements the 2-Stage Hierarchical Tree Architecture from
// Section 8.1.5 (Sarangi, Basic Computer Architecture):
//   - Stage I:  Bottom-up tree of (G, P) blocks (Figures 8.7 & 8.8)
//   - Stage II: Top-down carry distribution tree (Figure 8.9)
//   - Level 0:  16 2-bit Ripple Carry (RC) Adders for final sum bits
// ---------------------------------------------------------------------------
uint32_t detail::claAdd(uint32_t a, uint32_t b, uint32_t c_in, bool& cout, bool& overflow) {
    // =======================================================================
    // STEP 1: Compute 1-bit Propagate (p) and Generate (g) for all 32 bits
    // Equations 8.2 & 8.3:
    //   g_i = A_i AND B_i
    //   p_i = A_i XOR B_i
    // =======================================================================
    uint32_t g[32];
    uint32_t p[32];
    for (int i = 0; i < 32; i++) {
        uint32_t bit_a = (a >> i) & 1;
        uint32_t bit_b = (b >> i) & 1;
        g[i] = bit_a & bit_b;
        p[i] = bit_a ^ bit_b;
    }

    // =======================================================================
    // STAGE I: Bottom-Up Tree Reduction (Figure 8.8)
    // Combines sub-blocks using Equation 8.9:
    //   G_combined = G_high | (P_high & G_low)
    //   P_combined = P_high & P_low
    // =======================================================================

    // Level 1: 16 blocks of 2 bits each (bits 1-2, 3-4, ..., 31-32)
    uint32_t g1[16];
    uint32_t p1[16];
    for (int k = 0; k < 16; k++) {
        uint32_t g_low = g[2 * k];
        uint32_t p_low = p[2 * k];
        uint32_t g_high = g[2 * k + 1];
        uint32_t p_high = p[2 * k + 1];

        g1[k] = g_high | (p_high & g_low);
        p1[k] = p_high & p_low;
    }

    // Level 2: 8 blocks of 4 bits each (combine pairs of 2-bit blocks)
    uint32_t g2[8];
    uint32_t p2[8];
    for (int k = 0; k < 8; k++) {
        uint32_t g_low = g1[2 * k];
        uint32_t p_low = p1[2 * k];
        uint32_t g_high = g1[2 * k + 1];
        uint32_t p_high = p1[2 * k + 1];

        g2[k] = g_high | (p_high & g_low);
        p2[k] = p_high & p_low;
    }

    // Level 3: 4 blocks of 8 bits each (combine pairs of 4-bit blocks)
    uint32_t g3[4];
    uint32_t p3[4];
    for (int k = 0; k < 4; k++) {
        uint32_t g_low = g2[2 * k];
        uint32_t p_low = p2[2 * k];
        uint32_t g_high = g2[2 * k + 1];
        uint32_t p_high = p2[2 * k + 1];

        g3[k] = g_high | (p_high & g_low);
        p3[k] = p_high & p_low;
    }

    // Level 4: 2 blocks of 16 bits each (combine pairs of 8-bit blocks)
    uint32_t g4[2];
    uint32_t p4[2];
    for (int k = 0; k < 2; k++) {
        uint32_t g_low = g3[2 * k];
        uint32_t p_low = p3[2 * k];
        uint32_t g_high = g3[2 * k + 1];
        uint32_t p_high = p3[2 * k + 1];

        g4[k] = g_high | (p_high & g_low);
        p4[k] = p_high & p_low;
    }

    // Level 5: 1 block of 32 bits (combines the two 16-bit blocks)
    uint32_t g5 = g4[1] | (p4[1] & g4[0]);
    uint32_t p5 = p4[1] & p4[0];

    // =======================================================================
    // STAGE II: Top-Down Carry Distribution (Figure 8.9)
    // Uses Equation 8.4:
    //   C_out = G + P * C_in
    // Propagates carries downward so every 2-bit block receives its carry-in.
    // =======================================================================

    // Final carry-out of the full 32-bit adder (out of bit 31)
    cout = (g5 | (p5 & c_in)) != 0;

    // Carry out of lower 16-bit block (becomes carry into bit 16)
    uint32_t c16 = g4[0] | (p4[0] & c_in);

    // Carries at 8-bit boundaries (bits 8 and 24)
    uint32_t c8  = g3[0] | (p3[0] & c_in);
    uint32_t c24 = g3[2] | (p3[2] & c16);

    // Carries at 4-bit boundaries (bits 4, 12, 20, 28)
    uint32_t c4  = g2[0] | (p2[0] & c_in);
    uint32_t c12 = g2[2] | (p2[2] & c8);
    uint32_t c20 = g2[4] | (p2[4] & c16);
    uint32_t c28 = g2[6] | (p2[6] & c24);

    // Carries into each of the 16 2-bit blocks (at bits 0, 2, 4, ..., 30)
    uint32_t c_block[16];
    c_block[0]  = c_in;
    c_block[1]  = g1[0]  | (p1[0]  & c_in);
    c_block[2]  = c4;
    c_block[3]  = g1[2]  | (p1[2]  & c4);
    c_block[4]  = c8;
    c_block[5]  = g1[4]  | (p1[4]  & c8);
    c_block[6]  = c12;
    c_block[7]  = g1[6]  | (p1[6]  & c12);
    c_block[8]  = c16;
    c_block[9]  = g1[8]  | (p1[8]  & c16);
    c_block[10] = c20;
    c_block[11] = g1[10] | (p1[10] & c20);
    c_block[12] = c24;
    c_block[13] = g1[12] | (p1[12] & c24);
    c_block[14] = c28;
    c_block[15] = g1[14] | (p1[14] & c28);

    // =======================================================================
    // Level 0: 16 2-bit Ripple Carry (RC) Adders (Figure 8.9 top)
    // Computes the 2 sum bits for each block using its incoming carry.
    // =======================================================================
    uint32_t sum = 0;
    uint32_t c31 = 0; // Carry into the MSB (bit 31), needed for signed overflow

    for (int k = 0; k < 16; k++) {
        uint32_t cin_k = c_block[k];

        // First bit of the 2-bit block (bit 2*k)
        uint32_t sum_bit0 = p[2 * k] ^ cin_k;
        uint32_t carry_mid = g[2 * k] | (p[2 * k] & cin_k);

        // Second bit of the 2-bit block (bit 2*k + 1)
        uint32_t sum_bit1 = p[2 * k + 1] ^ carry_mid;

        // Place sum bits into the 32-bit result
        sum |= (sum_bit0 << (2 * k));
        sum |= (sum_bit1 << (2 * k + 1));

        // For the last block (k = 15, bits 30 and 31),
        // carry_mid is the carry into bit 31 (C_31).
        if (k == 15) {
            c31 = carry_mid;
        }
    }

    // Signed overflow: occurs if carry out of bit 31 != carry into bit 31
    overflow = (cout != (c31 != 0));

    return sum;
}

uint32_t detail::add32(uint32_t a, uint32_t b) {
    bool carry, overflow;
    return claAdd(a, b, 0, carry, overflow);
}

uint32_t detail::subtract32(uint32_t a, uint32_t b) {
    bool carry, overflow;
    return claAdd(a, ~b, 1, carry, overflow);
}

int32_t detail::addSigned32(int32_t a, int32_t b) {
    const uint32_t word = add32(static_cast<uint32_t>(a), static_cast<uint32_t>(b));
    int32_t value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

int32_t detail::subtractSigned32(int32_t a, int32_t b) {
    const uint32_t word = subtract32(static_cast<uint32_t>(a), static_cast<uint32_t>(b));
    int32_t value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

uint64_t detail::addSubtract33(uint64_t a, uint64_t b, bool subtract) {
    constexpr uint64_t mask = (uint64_t{1} << 33) - 1;
    const uint64_t operand = subtract ? (~b & mask) : b;
    bool carry, overflow;
    const uint32_t low = claAdd(static_cast<uint32_t>(a), static_cast<uint32_t>(operand),
                                subtract ? 1u : 0u, carry, overflow);
    const uint64_t high = ((a >> 32) ^ (operand >> 32) ^ uint64_t{carry}) & 1;
    return (high << 32) | low;
}

uint64_t detail::boothMultiply(uint32_t a, uint32_t b) {
    // A and M need 33 bits: -INT32_MIN cannot fit in a signed 32-bit word.
    constexpr uint64_t sign = uint64_t{1} << 32;
    const uint64_t M = uint64_t{a} | ((a & 0x80000000u) ? sign : 0);
    uint32_t Q = b;
    uint64_t A = 0;
    uint32_t Q_minus_1 = 0;

    for (int i = 0; i < 32; i++) {
        uint32_t Q_0 = Q & 1;
        if (Q_0 != Q_minus_1) {
            const bool subtract = Q_0 == 1;
            A = addSubtract33(A, M, subtract);
        }

        Q_minus_1 = Q & 1;
        uint32_t A_lsb = static_cast<uint32_t>(A & 1);
        Q = (Q >> 1) | (A_lsb << 31);
        A = (A >> 1) | (A & sign);
    }

    // Discard only the redundant sign-extension guard bit, not the high word.
    return (uint64_t{static_cast<uint32_t>(A)} << 32) | Q;
}

uint32_t Alu::booth_mul(uint32_t a, uint32_t b, bool& overflow) {
    const uint64_t product = detail::boothMultiply(a, b);
    const uint32_t low = static_cast<uint32_t>(product);
    const uint32_t expectedHigh = (low >> 31) ? 0xffffffffu : 0u;
    overflow = (product >> 32) != expectedHigh;
    return low;
}

detail::DivisionStep detail::restoringStep(uint64_t partial, uint32_t divisor) {
    const uint64_t trial = addSubtract33(partial, divisor, true);
    const bool negative = (trial & (uint64_t{1} << 32)) != 0;
    return {negative ? addSubtract33(trial, divisor, false) : trial, !negative};
}

detail::UnsignedDivisionResult detail::restoringDivide(uint32_t dividend, uint32_t divisor) {
    if (divisor == 0) {
        return {0, 0, ExceptionCode::DIV_BY_ZERO};
    }
    // The shared restoring step models the 33-bit trial subtraction and
    // restoration through the same CLA extension used by Booth multiplication.
    uint64_t A = 0;
    uint32_t Q = dividend;

    for (int i = 0; i < 32; i++) {
        uint32_t Q_msb = (Q >> 31) & 1;
        A = (A << 1) | Q_msb;
        Q = (Q << 1);

        const auto step = restoringStep(A, divisor);
        A = step.remainder;
        Q |= static_cast<uint32_t>(step.quotientBit);
    }
    return {Q, static_cast<uint32_t>(A), ExceptionCode::NONE};
}

uint32_t Alu::unsigned_div(uint32_t dividend, uint32_t divisor, ExceptionCode& exc) {
    const auto result = detail::restoringDivide(dividend, divisor);
    exc = result.exception;
    return result.quotient;
}

AluResult Alu::execute(AluOp op, uint32_t a, uint32_t b) {
    AluResult res;
    res.exception = ExceptionCode::NONE;
    bool cout = false;
    bool overflow = false;

    switch (op) {
        // CPU instructions use nearest-even; alternate modes and IEEE status
        // are available through fp32's API without changing integer FLAGS.
        case AluOp::FADD:
            res.value = fp32::add(a, b).value;
            break;
        case AluOp::FSUB:
            res.value = fp32::subtract(a, b).value;
            break;
        case AluOp::FMUL:
            res.value = fp32::multiply(a, b).value;
            break;
        case AluOp::FDIV:
            res.value = fp32::divide(a, b).value;
            break;
        case AluOp::ADD:
            res.value = detail::claAdd(a, b, 0, cout, overflow);
            res.flags.c = cout;
            res.flags.v = overflow;
            break;
        case AluOp::SUB:
        case AluOp::CMP:
            res.value = detail::claAdd(a, ~b, 1, cout, overflow);
            res.flags.c = cout;
            res.flags.v = overflow;
            break;
        case AluOp::MUL:
            res.value = booth_mul(a, b, overflow);
            res.flags.c = false; 
            res.flags.v = overflow;
            break;
        case AluOp::DIV:
            res.value = unsigned_div(a, b, res.exception);
            res.flags.c = false;
            res.flags.v = false;
            break;
        case AluOp::AND:
            res.value = a & b;
            res.flags.c = false; res.flags.v = false;
            break;
        case AluOp::OR:
            res.value = a | b;
            res.flags.c = false; res.flags.v = false;
            break;
        case AluOp::XOR:
            res.value = a ^ b;
            res.flags.c = false; res.flags.v = false;
            break;
        case AluOp::NOT:
            res.value = ~a;
            res.flags.c = false; res.flags.v = false;
            break;
        case AluOp::SHL:
            res.value = a << (b & 31);
            res.flags.c = false; res.flags.v = false;
            break;
        case AluOp::SHR:
            res.value = a >> (b & 31);
            res.flags.c = false; res.flags.v = false;
            break;
        case AluOp::MOV:
            res.value = a;
            res.flags.c = false; res.flags.v = false;
            break;
    }

    res.flags.z = (res.value == 0);
    res.flags.n = ((res.value >> 31) & 1) != 0;
    
    return res;
}

} // namespace risc201
