#include "simulator_core/pipeline.hpp"

#include "simulator_core/cpu.hpp"

namespace risc201 {

namespace {

// BEQ/BNE test Z, BLT/BGE compare the sign and overflow flags.
bool branchTaken(Opcode opcode, const Flags& flags) {
  switch (opcode) {
    case Opcode::BEQ:
      return flags.z;
    case Opcode::BNE:
      return !flags.z;
    case Opcode::BLT:
      return flags.n != flags.v;
    case Opcode::BGE:
      return flags.n == flags.v;
    default:
      return false;
  }
}

}  // namespace

Pipeline::Pipeline(Cpu& cpu, Alu& alu, ControlUnit& control, MemorySafety& safety)
    : cpu_(cpu), alu_(alu), control_(control), safety_(safety) {}

void Pipeline::step() {
  fetch();
  if (stopped()) {
    return;
  }

  decode();
  if (stopped()) {
    return;
  }

  execute();
  if (stopped()) {
    return;
  }

  writeBack();
}

void Pipeline::reset() {
  ifId_ = IfId{};
  idEx_ = IdEx{};
  exWb_ = ExWb{};
}

bool Pipeline::stopped() const { return cpu_.exception() != ExceptionCode::NONE; }

void Pipeline::fetch() {
  // PC is only updated in write back, so it still points at the faulting
  // instruction when something goes wrong.
  if (cpu_.pc() > kCodeLimit) {
    cpu_.setException(ExceptionCode::PC_OUT_OF_RANGE);
    return;
  }

  ifId_.pc = cpu_.pc();
  ifId_.instruction = cpu_.readMem(ifId_.pc);
}

void Pipeline::decode() {
  idEx_ = IdEx{};
  idEx_.instruction = ifId_.instruction;
  idEx_.pc = ifId_.pc;
  idEx_.signals = control_.decode(idEx_.instruction);

  if (idEx_.signals.illegal) {
    cpu_.setException(ExceptionCode::ILLEGAL_INSTRUCTION);
    return;
  }

  const uint8_t rs = decodeRs(idEx_.instruction);
  const uint8_t rt = decodeRt(idEx_.instruction);

  idEx_.operandA = cpu_.reg(rs);
  idEx_.operandB = idEx_.signals.aluSrc == AluSrc::Immediate
                       ? static_cast<uint32_t>(decodeImmediate(idEx_.instruction))
                       : cpu_.reg(rt);

  if (idEx_.signals.opcode == Opcode::LDI) {
    // LDI has no source register, the immediate is the whole value.
    idEx_.operandA = 0;
  }
}

void Pipeline::execute() {
  exWb_ = ExWb{};

  const ControlSignals& signals = idEx_.signals;
  exWb_.signals = signals;
  exWb_.instruction = idEx_.instruction;
  exWb_.pc = idEx_.pc;

  if (signals.halt) {
    return;
  }

  const AluResult aluResult = alu_.execute(signals.aluOp, idEx_.operandA, idEx_.operandB);
  if (aluResult.exception != ExceptionCode::NONE) {
    cpu_.setException(aluResult.exception);
    return;
  }
  exWb_.result = aluResult.value;
  exWb_.flags = aluResult.flags;

  if (signals.push) {
    // PUSH and CALL put the item at the new top of the stack.
    const ExceptionCode stack = safety_.checkStack(cpu_.sp(), true);
    if (stack != ExceptionCode::NONE) {
      cpu_.setException(stack);
      return;
    }
    exWb_.memoryAddress = cpu_.sp() - 1;
    exWb_.storeValue = signals.opcode == Opcode::CALL ? idEx_.pc + 1
                                                      : cpu_.reg(decodeRd(idEx_.instruction));
    exWb_.nextSp = cpu_.sp() - 1;
    exWb_.updateSp = true;
  } else if (signals.pop) {
    const ExceptionCode stack = safety_.checkStack(cpu_.sp(), false);
    if (stack != ExceptionCode::NONE) {
      cpu_.setException(stack);
      return;
    }
    exWb_.memoryAddress = cpu_.sp();
    exWb_.nextSp = cpu_.sp() + 1;
    exWb_.updateSp = true;
  } else if (signals.ret) {
    const ExceptionCode stack = safety_.checkStack(cpu_.sp(), false);
    if (stack != ExceptionCode::NONE) {
      cpu_.setException(stack);
      return;
    }
    // RET jumps to the address sitting on top of the stack.
    exWb_.target = cpu_.readMem(cpu_.sp());
    exWb_.nextSp = cpu_.sp() + 1;
    exWb_.updateSp = true;
    exWb_.redirect = true;
  } else if (signals.memRead || signals.memWrite) {
    // LW and SW add the immediate to the base register.
    const ExceptionCode memory = safety_.checkMem(aluResult.value, signals.memWrite);
    if (memory != ExceptionCode::NONE) {
      cpu_.setException(memory);
      return;
    }
    exWb_.memoryAddress = aluResult.value;
    exWb_.storeValue = cpu_.reg(decodeRd(idEx_.instruction));
  }

  if (signals.jump || (signals.branch && branchTaken(signals.opcode, cpu_.flags()))) {
    exWb_.target = idEx_.pc + 1 + static_cast<uint32_t>(decodeOffset(idEx_.instruction));
    exWb_.redirect = true;
  }
}

void Pipeline::writeBack() {
  const ControlSignals& signals = exWb_.signals;

  if (signals.halt) {
    cpu_.setHalted(true);
    return;
  }

  if (signals.memWrite) {
    cpu_.writeMem(exWb_.memoryAddress, exWb_.storeValue);
  }

  uint32_t value = exWb_.result;
  if (signals.resultSrc == ResultSrc::Memory) {
    value = cpu_.readMem(exWb_.memoryAddress);
  }

  if (signals.regWrite) {
    cpu_.setReg(decodeRd(exWb_.instruction), value);
  }

  if (signals.writeFlags) {
    cpu_.setFlags(exWb_.flags);
  }

  if (exWb_.updateSp) {
    cpu_.setSp(exWb_.nextSp);
  }

  cpu_.setPc(exWb_.redirect ? exWb_.target : exWb_.pc + 1);
}

}  // namespace risc201
