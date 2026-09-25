#include "simulator_core/cpu.hpp"

#include <cassert>
#include <cstddef>

namespace risc201 {

Cpu::Cpu() { reset(); }

void Cpu::load(const std::vector<uint32_t>& program) {
  reset();
  memory_.fill(0);

  const std::size_t count = program.size() < kMemoryWords ? program.size() : kMemoryWords;
  for (std::size_t i = 0; i < count; ++i) {
    memory_[i] = program[i];
  }
}

void Cpu::step() {
  // Nothing executes the instruction yet, so a step only fetches and moves on.
  assert(pc_ <= kCodeLimit);
  pc_ = pc_ + 1;
}

void Cpu::run() {
  while (!halted_ && pc_ <= kCodeLimit) {
    step();
  }
}

void Cpu::reset() {
  registers_.fill(0);
  pc_ = kCodeBase;
  sp_ = kInitialSp;
  flags_ = Flags{};
  halted_ = false;
}

uint32_t Cpu::reg(uint8_t index) const {
  assert(index < kRegisterCount);
  return registers_[index];
}

void Cpu::setReg(uint8_t index, uint32_t value) {
  assert(index < kRegisterCount);
  registers_[index] = value;
}

uint32_t Cpu::pc() const { return pc_; }

void Cpu::setPc(uint32_t value) { pc_ = value; }

uint32_t Cpu::sp() const { return sp_; }

void Cpu::setSp(uint32_t value) { sp_ = value; }

Flags Cpu::flags() const { return flags_; }

void Cpu::setFlags(const Flags& value) { flags_ = value; }

uint32_t Cpu::readMem(uint32_t address) const {
  assert(address <= kLastAddress);
  return memory_[address];
}

void Cpu::writeMem(uint32_t address, uint32_t value) {
  assert(address <= kLastAddress);
  memory_[address] = value;
}

bool Cpu::halted() const { return halted_; }

}  // namespace risc201
