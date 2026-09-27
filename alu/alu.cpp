#include "alu/alu.hpp"

namespace risc201 {

// We create a concrete class that inherits from the abstract Alu class.
// We put it in an anonymous namespace so it doesn't pollute the global scope.
namespace {

class ConcreteAlu : public Alu {
private:
    Flags compute_flags(uint32_t a, uint32_t b, uint32_t value, bool is_add, bool is_sub, bool is_logical) {
        Flags f;
        f.z = (value == 0) ? true : false;
        f.n = ((value >> 31) & 1) != 0;

        if (is_add) {
            f.c = (value < a) ? true : false;
            f.v = (((~(a ^ b) & (a ^ value)) >> 31) & 1) != 0;
        } else if (is_sub) {
            f.c = (a < b) ? true : false;
            f.v = ((((a ^ b) & (a ^ value)) >> 31) & 1) != 0;
        } else {
            f.c = false;
            f.v = false;
        }
        return f;
    }

public:
    AluResult execute(AluOp op, uint32_t a, uint32_t b) override {
        AluResult res; // Initializes with value=0, flags=false, exception=NONE
        
        switch (op) {
            case AluOp::ADD:
                res.value = a + b;
                res.flags = compute_flags(a, b, res.value, true, false, false);
                break;
            case AluOp::SUB:
            case AluOp::CMP:
                res.value = a - b;
                res.flags = compute_flags(a, b, res.value, false, true, false);
                break;
            case AluOp::MUL:
                res.value = a * b;
                res.flags = compute_flags(a, b, res.value, false, false, false);
                break;
            case AluOp::DIV:
                if (b == 0) {
                    res.exception = ExceptionCode::DIV_BY_ZERO;
                } else {
                    res.value = a / b;
                    res.flags = compute_flags(a, b, res.value, false, false, false);
                }
                break;
            case AluOp::AND:
                res.value = a & b;
                res.flags = compute_flags(a, b, res.value, false, false, true);
                break;
            case AluOp::OR:
                res.value = a | b;
                res.flags = compute_flags(a, b, res.value, false, false, true);
                break;
            case AluOp::XOR:
                res.value = a ^ b;
                res.flags = compute_flags(a, b, res.value, false, false, true);
                break;
            case AluOp::NOT:
                res.value = ~a;
                res.flags = compute_flags(a, 0, res.value, false, false, true);
                break;
            case AluOp::SHL:
                res.value = a << (b & 31);
                res.flags = compute_flags(a, b, res.value, false, false, true);
                break;
            case AluOp::SHR:
                res.value = a >> (b & 31);
                res.flags = compute_flags(a, b, res.value, false, false, true);
                break;
            case AluOp::MOV:
                res.value = a;
                res.flags = compute_flags(a, 0, res.value, false, false, true);
                break;
        }
        return res;
    }
};

} // anonymous namespace

// Factory function or global instance so tests can access the concrete implementation
Alu* createAlu() {
    return new ConcreteAlu();
}

} // namespace risc201