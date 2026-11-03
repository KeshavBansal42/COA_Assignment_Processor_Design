#include "alu/float32.hpp"
#include "alu/alu.hpp"

#include <utility>

namespace risc201::fp32 {
namespace {

constexpr uint32_t kSign = 0x80000000u;
constexpr uint32_t kFraction = 0x007fffffu;
constexpr uint32_t kHidden = 0x00800000u;
constexpr uint32_t kInfinity = 0x7f800000u;
constexpr uint32_t kMaxFinite = 0x7f7fffffu;
constexpr uint32_t kQuiet = 0x00400000u;
constexpr uint32_t kNaN = 0x7fc00000u;

struct Parts {
    bool sign;
    unsigned field;
    uint32_t significand;
    int exponent;

    bool nan() const { return field == 255 && significand != 0; }
    bool signaling() const { return nan() && !(significand & kQuiet); }
    bool infinity() const { return field == 255 && significand == 0; }
    bool zero() const { return field == 0 && significand == 0; }
};

Parts unpack(uint32_t word) {
    const unsigned field = (word >> 23) & 255;
    return {(word & kSign) != 0, field, word & kFraction,
            field == 0 ? -126 : detail::subtractSigned32(static_cast<int32_t>(field), 127)};
}

// Convert finite nonzero inputs, including subnormals, to a 24-bit significand
// with its leading one at bit 23. No precision is discarded here.
void normalize(Parts& p) {
    if (p.field != 0) p.significand |= kHidden;
    while (p.significand < kHidden) {
        p.significand <<= 1;
        p.exponent = detail::subtractSigned32(p.exponent, 1);
    }
}

uint32_t signedWord(bool sign, uint32_t magnitude) {
    return (sign ? kSign : 0u) | magnitude;
}

Result invalidResult() {
    return {kNaN, {true, false, false, false, false}};
}

Result nanResult(const Parts& a, const Parts& b) {
    return {kNaN, {a.signaling() || b.signaling(), false, false, false, false}};
}

// Preserve all discarded information as one sticky bit, including shifts
// larger than a word. This is essential for directed rounding and tiny inputs.
uint64_t shiftRightJam(uint64_t value, unsigned distance) {
    if (distance == 0) return value;
    if (distance >= 64) return value != 0;
    return (value >> distance) | ((value << (64 - distance)) != 0);
}

// sig has three extra guard/round/sticky bits below its 24-bit significand.
// Normalize, make subnormal if needed, round once, then pack the binary32 word.
Result roundPack(bool sign, int exponent, uint64_t sig, RoundingMode mode) {
    if (sig == 0) return {signedWord(sign, 0), {}};
    while (sig >= (uint64_t{1} << 27)) {
        sig = shiftRightJam(sig, 1);
        exponent = detail::addSigned32(exponent, 1);
    }
    while (sig < (uint64_t{1} << 26)) {
        sig <<= 1;
        exponent = detail::subtractSigned32(exponent, 1);
    }
    if (exponent < -126) {
        sig = shiftRightJam(sig, static_cast<unsigned>(detail::subtractSigned32(-126, exponent)));
        exponent = -126;
    }

    const unsigned tail = static_cast<unsigned>(sig & 7);
    uint32_t rounded = static_cast<uint32_t>(sig >> 3);
    bool increment = false;
    switch (mode) {
        case RoundingMode::NearestEven:
            increment = tail > 4 || (tail == 4 && (rounded & 1));
            break;
        case RoundingMode::TowardPositive: increment = !sign && tail != 0; break;
        case RoundingMode::TowardNegative: increment = sign && tail != 0; break;
        case RoundingMode::TowardZero: break;
    }
    rounded = detail::add32(rounded, static_cast<uint32_t>(increment));
    if (rounded >= (kHidden << 1)) {
        rounded >>= 1;
        exponent = detail::addSigned32(exponent, 1);
    }

    Status status{};
    status.inexact = tail != 0;
    if (exponent > 127) {
        status.overflow = status.inexact = true;
        const bool infinity = mode == RoundingMode::NearestEven ||
            (mode == RoundingMode::TowardPositive && !sign) ||
            (mode == RoundingMode::TowardNegative && sign);
        return {signedWord(sign, infinity ? kInfinity : kMaxFinite), status};
    }

    const bool subnormal = rounded < kHidden;
    status.underflow = subnormal && status.inexact;
    const uint32_t field = subnormal ? 0u : static_cast<uint32_t>(detail::addSigned32(exponent, 127));
    return {signedWord(sign, (field << 23) | (rounded & kFraction)), status};
}

Result addOrSubtract(uint32_t aWord, uint32_t bWord, bool subtract, RoundingMode mode) {
    Parts a = unpack(aWord), b = unpack(bWord);
    if (a.nan() || b.nan()) return nanResult(a, b);
    b.sign ^= subtract;
    if (a.infinity() || b.infinity()) {
        if (a.infinity() && b.infinity() && a.sign != b.sign) return invalidResult();
        return {signedWord(a.infinity() ? a.sign : b.sign, kInfinity), {}};
    }
    if (a.zero() && b.zero()) {
        const bool sign = a.sign == b.sign ? a.sign : mode == RoundingMode::TowardNegative;
        return {signedWord(sign, 0), {}};
    }
    if (a.zero()) return {signedWord(b.sign, bWord & ~kSign), {}};
    if (b.zero()) return {aWord, {}};

    normalize(a);
    normalize(b);
    // a is the larger magnitude, hence determines the nonzero result's sign.
    if (a.exponent < b.exponent ||
        (a.exponent == b.exponent && a.significand < b.significand)) std::swap(a, b);
    // 24 significand bits plus G/R/S, with one extra sum bit, fit in the CLA.
    const uint32_t left = a.significand << 3;
    const uint32_t right = static_cast<uint32_t>(shiftRightJam(uint64_t{b.significand} << 3,
        static_cast<unsigned>(detail::subtractSigned32(a.exponent, b.exponent))));
    const uint32_t sig = a.sign == b.sign ? detail::add32(left, right)
                                         : detail::subtract32(left, right);
    if (sig == 0) return {signedWord(mode == RoundingMode::TowardNegative, 0), {}};
    return roundPack(a.sign, a.exponent, sig, mode);
}

} // namespace

Result add(uint32_t a, uint32_t b, RoundingMode mode) {
    return addOrSubtract(a, b, false, mode);
}

Result subtract(uint32_t a, uint32_t b, RoundingMode mode) {
    return addOrSubtract(a, b, true, mode);
}

Result multiply(uint32_t aWord, uint32_t bWord, RoundingMode mode) {
    Parts a = unpack(aWord), b = unpack(bWord);
    if (a.nan() || b.nan()) return nanResult(a, b);
    const bool sign = a.sign != b.sign;
    if ((a.infinity() && b.zero()) || (b.infinity() && a.zero())) return invalidResult();
    if (a.infinity() || b.infinity()) return {signedWord(sign, kInfinity), {}};
    if (a.zero() || b.zero()) return {signedWord(sign, 0), {}};
    normalize(a);
    normalize(b);

    // The shared Wallace tree preserves the complete 48-bit significand product;
    // the public integer MUL's low word would be insufficient.
    const uint64_t product = detail::wallaceMultiply(a.significand, b.significand);
    // The product's binary point is after bit 46. Retain 24+3 bits.
    return roundPack(sign, detail::addSigned32(a.exponent, b.exponent),
                     shiftRightJam(product, 20), mode);
}

Result divide(uint32_t aWord, uint32_t bWord, RoundingMode mode) {
    Parts a = unpack(aWord), b = unpack(bWord);
    if (a.nan() || b.nan()) return nanResult(a, b);
    const bool sign = a.sign != b.sign;
    if ((a.zero() && b.zero()) || (a.infinity() && b.infinity())) return invalidResult();
    if (a.infinity()) return {signedWord(sign, kInfinity), {}};
    if (b.infinity()) return {signedWord(sign, 0), {}};
    if (b.zero()) return {signedWord(sign, kInfinity), {false, true, false, false, false}};
    if (a.zero()) return {signedWord(sign, 0), {}};
    normalize(a);
    normalize(b);
    int exponent = detail::subtractSigned32(a.exponent, b.exponent);
    uint32_t numerator = a.significand;
    if (numerator < b.significand) {
        numerator <<= 1;
        exponent = detail::subtractSigned32(exponent, 1);
    }

    // NR refines the reciprocal using shared Wallace/CLA primitives. Exact
    // residual correction recovers the 27 quotient bits and the true sticky bit.
    const auto divided = detail::newtonDivideSignificands(numerator, b.significand);
    uint64_t quotient = divided.quotient;
    if (divided.remainder != 0) quotient |= 1;
    return roundPack(sign, exponent, quotient, mode);
}

} // namespace risc201::fp32
