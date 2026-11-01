#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "assembler/assembler.hpp"
#include "disassembler/disassembler.hpp"
#include "simulator_core/cpu.hpp"

using namespace risc201;

static void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::abort();
  }
}

static uint32_t bits(float value) {
  uint32_t word;
  std::memcpy(&word, &value, sizeof(word));
  return word;
}

int main() {
  const auto path = std::filesystem::temp_directory_path() / "risc201_float_test.s";
  {
    std::ofstream source(path);
    source << "FADD R3, R1, R2\nFSUB R4, R3, R2\nFMUL R5, R4, R2\n"
              "FDIV R6, R5, R2\nFDIV R7, R1, R0\nHALT\n";
  }
  std::vector<uint32_t> program;
  assemble(path.string(), program);
  check(disassemble(program[0]) == "FADD R3, R1, R2", "FADD disassembly");
  std::filesystem::remove(path);
  auto hex = path; hex.replace_extension(".hex"); std::filesystem::remove(hex);

  for (auto mode : {PipelineMode::Sequential, PipelineMode::FourStage, PipelineMode::SixStage}) {
    for (auto control : {ControlMode::Horizontal, ControlMode::Vertical}) {
      Cpu cpu;
      cpu.load(program);
      cpu.setPipelineMode(mode);
      cpu.setControlMode(control);
      cpu.setReg(1, bits(1.5f));
      cpu.setReg(2, bits(2.0f));
      cpu.setFlags({true, false, true, false});
      for (int i = 0; i < 200 && !cpu.halted() && cpu.exception() == ExceptionCode::NONE; ++i) cpu.step();
      check(cpu.halted() && cpu.exception() == ExceptionCode::NONE, "floating program did not halt");
      check(cpu.reg(3) == bits(3.5f), "FADD dependency");
      check(cpu.reg(4) == bits(1.5f), "FSUB dependency");
      check(cpu.reg(5) == bits(3.0f), "FMUL dependency");
      check(cpu.reg(6) == bits(1.5f), "FDIV dependency");
      check(cpu.reg(7) == bits(INFINITY), "FDIV by zero should return infinity");
      check(cpu.flags() == (Flags{true, false, true, false}), "FP changed integer FLAGS");

      struct Case { Opcode op; uint32_t a, b, expected; };
      const Case cases[] = {
        {Opcode::FADD, 0x3f800000, 0x33800000, 0x3f800000}, // halfway -> even
        {Opcode::FADD, 0x007fffff, 1, 0x00800000},          // subnormal -> normal
        {Opcode::FSUB, 0x3f800000, 0x3f800000, 0},
        {Opcode::FMUL, 0x80000000, 0x40000000, 0x80000000},
        {Opcode::FMUL, 0x7f7fffff, 0x40000000, 0x7f800000}, // overflow -> infinity
        {Opcode::FDIV, 1, 0x40000000, 0},                  // underflow -> zero
        {Opcode::FDIV, 0x00800000, 0x40000000, 0x00400000}, // exact subnormal
        {Opcode::FDIV, 0, 0, 0x7fc00000},                  // invalid -> quiet NaN
        {Opcode::FADD, 0x7f800000, 0xff800000, 0x7fc00000},
        {Opcode::FSUB, 0x7f812345, 0x3f800000, 0x7fc00000}, // signaling NaN
      };
      for (const auto& test : cases) {
        Cpu special;
        special.load({encodeR(test.op, 3, 1, 2), encodeOpcode(Opcode::HALT)});
        special.setPipelineMode(mode);
        special.setControlMode(control);
        special.setReg(1, test.a);
        special.setReg(2, test.b);
        const Flags saved{true, false, true, true};
        special.setFlags(saved);
        for (int i = 0; i < 50 && !special.halted() &&
             special.exception() == ExceptionCode::NONE; ++i) special.step();
        check(special.halted() && special.exception() == ExceptionCode::NONE,
              "FP special value caused a processor exception");
        check(special.reg(3) == test.expected, "FP special value result");
        check(special.flags() == saved, "FP special value changed integer FLAGS");
      }
    }
  }
}
