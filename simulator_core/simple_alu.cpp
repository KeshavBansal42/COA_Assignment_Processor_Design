#include "simulator_core/simple_alu.hpp"

namespace risc201 {

namespace {

constexpr uint32_t kSignBit = 0x80000000u;

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

    case AluOp::SUB: {
      const uint64_t wide = static_cast<uint64_t>(a) - static_cast<uint64_t>(b);
      result.value = static_cast<uint32_t>(wide);
      result.flags.z = result.value == 0;
      result.flags.n = (result.value & kSignBit) != 0;
      result.flags.c = a >= b;
      result.flags.v = ((a ^ b) & (a ^ result.value) & kSignBit) != 0;
      break;
    }

    default:
      break;
  }

  return result;
}

}  // namespace risc201
