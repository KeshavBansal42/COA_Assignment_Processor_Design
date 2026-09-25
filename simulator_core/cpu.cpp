#include "simulator_core/cpu.hpp"

#include <cassert>
#include <cstddef>

namespace risc201 {

Cpu::Cpu() : pipeline_(*this, alu_, control_, safety_) { reset(); }

void Cpu::load(const std::vector<uint32_t>& program) {
  reset();
  memory_.fill(0);

  const std::size_t count = program.size() < kMemoryWords ? program.size() : kMemoryWords;
  for (std::size_t i = 0; i < count; ++i) {
    memory_[i] = program[i];
  }
}

void Cpu::step() {
  if (halted_ || exception_ != ExceptionCode::NONE) {
    return;
  }
  wroteMemory_ = false;
  pipeline_.step();
}

void Cpu::run() {
  while (!halted_ && exception_ == ExceptionCode::NONE) {
    step();
  }
}

void Cpu::reset() {
  registers_.fill(0);
  pc_ = kCodeBase;
  sp_ = kInitialSp;
  flags_ = Flags{};
  halted_ = false;
  exception_ = ExceptionCode::NONE;
  wroteMemory_ = false;
  lastWriteAddress_ = 0;
  pipeline_.reset();
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
  wroteMemory_ = true;
  lastWriteAddress_ = address;
}

bool Cpu::halted() const { return halted_; }

void Cpu::setHalted(bool value) { halted_ = value; }

ExceptionCode Cpu::exception() const { return exception_; }

void Cpu::setException(ExceptionCode value) { exception_ = value; }

bool Cpu::wroteMemory() const { return wroteMemory_; }

uint32_t Cpu::lastWriteAddress() const { return lastWriteAddress_; }

}  // namespace risc201
