#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assembler/assembler.hpp"
#include "disassembler/disassembler.hpp"
#include "simulator_core/cpu.hpp"

using namespace risc201;

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
  assert(disassemble(program[0]) == "FADD R3, R1, R2");
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
      assert(cpu.halted() && cpu.exception() == ExceptionCode::NONE);
      assert(cpu.reg(3) == bits(3.5f));
      assert(cpu.reg(4) == bits(1.5f));
      assert(cpu.reg(5) == bits(3.0f));
      assert(cpu.reg(6) == bits(1.5f));
      assert(cpu.reg(7) == bits(INFINITY));
      assert((cpu.flags() == Flags{true, false, true, false}));
    }
  }
}
