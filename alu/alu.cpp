#include "alu/alu.hpp"

namespace risc201 {

uint32_t Alu::cla_add(uint32_t a, uint32_t b, uint32_t c_in, bool& cout, bool& overflow) {
    uint32_t p = a ^ b; // Propagate
    uint32_t g = a & b; // Generate
    uint32_t c = c_in;
    uint32_t sum = 0;
    bool c_prev = c_in;

    for (int i = 0; i < 32; i++) {
        uint32_t pi = (p >> i) & 1;
        uint32_t gi = (g >> i) & 1;
        uint32_t ci = (c >> i) & 1;

        sum |= (pi ^ ci) << i;

        uint32_t next_c = gi | (pi & ci);
        c_prev = ci; 
        
        if (i < 31) {
            c |= (next_c << (i + 1));
        } else {
            cout = next_c;
        }
    }
    overflow = cout ^ c_prev;
    return sum;
}

uint32_t Alu::booth_mul(uint32_t a, uint32_t b, bool& overflow) {
    uint32_t M = a;
    uint32_t Q = b;
    uint32_t A = 0;
    uint32_t Q_minus_1 = 0;

    for (int i = 0; i < 32; i++) {
        uint32_t Q_0 = Q & 1;
        bool dummy_c, dummy_v; 
        
        if (Q_0 == 1 && Q_minus_1 == 0) {
            A = cla_add(A, ~M, 1, dummy_c, dummy_v); 
        } else if (Q_0 == 0 && Q_minus_1 == 1) {
            A = cla_add(A, M, 0, dummy_c, dummy_v);  
        }

        Q_minus_1 = Q & 1;
        uint32_t A_lsb = A & 1;
        uint32_t A_msb = A & 0x80000000; 
        Q = (Q >> 1) | (A_lsb << 31);
        A = (A >> 1) | A_msb;
    }

    // Overflow check: in 64-bit product [A:Q], A must be sign extension of Q
    uint32_t q_sign = (Q >> 31) & 1;
    uint32_t expected_A = q_sign ? 0xFFFFFFFFu : 0u;
    overflow = (A != expected_A);

    return Q; 
}

uint32_t Alu::unsigned_div(uint32_t dividend, uint32_t divisor, ExceptionCode& exc) {
    if (divisor == 0) {
        exc = ExceptionCode::DIV_BY_ZERO;
        return 0;
    }
    // 33-bit / 64-bit accumulator A to hold bits shifted from Q without losing the MSB
    uint64_t A = 0;
    uint32_t M = divisor;
    uint32_t Q = dividend;

    for (int i = 0; i < 32; i++) {
        uint32_t Q_msb = (Q >> 31) & 1;
        A = (A << 1) | Q_msb;
        Q = (Q << 1);

        if (A >= M) {
            A -= M;
            Q |= 1;
        } else {
            Q &= ~1u;
        }
    }
    return Q; 
}

AluResult Alu::execute(AluOp op, uint32_t a, uint32_t b) {
    AluResult res;
    res.exception = ExceptionCode::NONE;
    bool cout = false;
    bool overflow = false;

    switch (op) {
        case AluOp::ADD:
            res.value = cla_add(a, b, 0, cout, overflow);
            res.flags.c = cout;
            res.flags.v = overflow;
            break;
        case AluOp::SUB:
        case AluOp::CMP:
            res.value = cla_add(a, ~b, 1, cout, overflow); 
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