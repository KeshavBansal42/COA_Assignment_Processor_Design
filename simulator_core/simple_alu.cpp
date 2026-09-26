#include "simulator_core/simple_alu.hpp"

namespace risc201 {

namespace {

constexpr uint32_t kSignBit = 0x80000000u;
constexpr uint32_t kShiftMask = 31u;

}  // namespace

AluResult SimpleAlu::execute(AluOp op, uint32_t a, uint32_t b) {
  AluResult result;

  switch (op) {
    case AluOp::ADD: {
      const uint64_t wide = static_cast<uint64_t>(a) + static_cast<uint64_t>(b);
      result.value = static_cast<uint32_t>(wide);
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      result.flags.c = wide > 0xFFFFFFFFull;
      result.flags.v = ((a ^ result.value) & (b ^ result.value) & kSignBit) != 0;
      break;
    }

    case AluOp::SUB:
    case AluOp::CMP: {
      const uint64_t wide = static_cast<uint64_t>(a) - static_cast<uint64_t>(b);
      result.value = static_cast<uint32_t>(wide);
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      result.flags.c = a >= b;
      result.flags.v = ((a ^ b) & (a ^ result.value) & kSignBit) != 0;
      break;
    }

    case AluOp::MUL: {
      const int64_t product = static_cast<int64_t>(static_cast<int32_t>(a)) *
                              static_cast<int64_t>(static_cast<int32_t>(b));
      result.value = static_cast<uint32_t>(product);
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      result.flags.v = product != static_cast<int64_t>(static_cast<int32_t>(result.value));
      break;
    }

    case AluOp::DIV: {
      if (b == 0) {
        result.exception = ExceptionCode::DIV_BY_ZERO;
        break;
      }
      const int32_t dividend = static_cast<int32_t>(a);
      const int32_t divisor = static_cast<int32_t>(b);
      result.value = static_cast<uint32_t>(dividend / divisor);
      break;
    }

    case AluOp::AND:
      result.value = a & b;
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      break;

    case AluOp::OR:
      result.value = a | b;
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      break;

    case AluOp::XOR:
      result.value = a ^ b;
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      break;

    case AluOp::NOT:
      result.value = ~a;
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      break;

    case AluOp::SHL:
      result.value = a << (b & kShiftMask);
      break;

    case AluOp::SHR:
      result.value = a >> (b & kShiftMask);
      break;

    case AluOp::MOV:
      result.value = a;
      break;
  }

  return result;
}

}  // namespace risc201
