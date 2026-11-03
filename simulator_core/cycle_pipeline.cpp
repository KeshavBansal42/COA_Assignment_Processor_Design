#include "simulator_core/cycle_pipeline.hpp"

#include "alu/alu.hpp"
#include "control_unit/control_unit.hpp"
#include "memory_safety/memory_safety.hpp"
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

CyclePipeline::CyclePipeline(Cpu& cpu, Alu& alu, ControlUnit& control, MemorySafety& safety,
                             int stages)
    : cpu_(cpu), alu_(alu), control_(control), safety_(safety), stages_(stages) {
  reset();
}

void CyclePipeline::reset() {
  for (PipeLatch& latch : lat_) {
    latch = PipeLatch{};
  }
  fetchPc_ = cpu_.pc();
  draining_ = false;
  done_ = false;
  stopPc_ = 0;
  stopHalt_ = false;
  stopException_ = ExceptionCode::NONE;
  cycles_ = 0;
  retired_ = 0;
  stalls_ = 0;
  flushes_ = 0;
  forwards_ = 0;
}

int CyclePipeline::operandStage() const { return stages_ == 4 ? 1 : 2; }

int CyclePipeline::executeStage() const { return stages_ == 4 ? 2 : 3; }

int CyclePipeline::memoryStage() const { return stages_ == 4 ? -1 : 4; }

bool CyclePipeline::empty() const {
  for (const PipeLatch& latch : lat_) {
    if (latch.valid) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> CyclePipeline::stageNames() const {
  if (stages_ == 4) {
    return {"ID", "EX", "WB", "IF"};
  }
  return {"ID", "OF", "EX", "MEM", "WB", "IF"};
}

uint32_t CyclePipeline::immediateOf(uint32_t instr) const {
  return static_cast<uint32_t>(decodeImmediate(instr));
}

PipeLatch CyclePipeline::fetchLatch() {
  PipeLatch out;
  out.valid = true;
  out.pc = fetchPc_;
  out.instr = cpu_.readMem(fetchPc_);
  ++fetchPc_;
  return out;
}

// Decode only reads the instruction, operands come later.
PipeLatch CyclePipeline::decodeLatch(const PipeLatch& in) {
  PipeLatch out = in;
  out.signals = control_.decode(in.instr);

  if (out.signals.illegal) {
    out.stop = true;
    out.stopException = ExceptionCode::ILLEGAL_INSTRUCTION;
    return out;
  }

  const ControlSignals& s = out.signals;
  out.writesReg = s.regWrite;
  out.destReg = s.regWrite ? decodeRd(in.instr) : 0xFF;
  out.writesFlags = s.writeFlags;
  out.writesSp = s.push || s.pop || s.ret;
  out.isLoad = s.regWrite && s.resultSrc == ResultSrc::Memory;
  return out;
}

// Read the register operands, forwarding from anything already computed by an
// older instruction that is still in flight.
PipeLatch CyclePipeline::operandLatch(const PipeLatch& in,
                                      const std::array<PipeLatch, kMaxStages>& ahead) {
  const int here = operandStage();
  PipeLatch out = in;
  const ControlSignals& s = in.signals;

  const uint8_t rs = decodeRs(in.instr);
  const uint8_t rt = decodeRt(in.instr);

  out.sp = cpu_.sp();
  out.operandA = forwardRegister(rs, ahead, here);
  out.operandB = s.aluSrc == AluSrc::Immediate ? immediateOf(in.instr)
                                               : forwardRegister(rt, ahead, here);

  // Stores and pushes take their value from rd, which is a normal read at the
  // operand stage and has to be forwarded like any other operand.
  if (s.opcode == Opcode::SW || s.opcode == Opcode::PUSH) {
    out.storeData = forwardRegister(decodeRd(in.instr), ahead, here);
  }

  if (s.opcode == Opcode::LDI) {
    // LDI has no source register, the immediate is the whole value.
    out.operandA = 0;
  }
  return out;
}

// The value of a register as seen by the operand stage: the newest in-flight
// writer that is ready, otherwise the committed register file.
uint32_t CyclePipeline::forwardRegister(uint8_t reg,
                                        const std::array<PipeLatch, kMaxStages>& ahead,
                                        int from) {
  for (int i = from + 1; i <= stages_ - 1; ++i) {
    const PipeLatch& p = ahead[i];
    if (!p.valid || !p.writesReg || p.destReg != reg) {
      continue;
    }
    if (p.resultReady) {
      ++forwards_;
      return p.resultValue;
    }
    // Not ready means detectStall should have held this instruction.
    return cpu_.reg(reg);
  }
  return cpu_.reg(reg);
}

// A younger instruction may not read an operand whose producer is still in
// flight and not ready, and a branch has to wait for its flags, a stack access
// for the stack pointer.
bool CyclePipeline::detectStall(const PipeLatch& in,
                                const std::array<PipeLatch, kMaxStages>& ahead, int from) {
  if (!in.valid) {
    return false;
  }

  // Decode here rather than reading in.signals: in the 4 stage pipeline the
  // operand read happens in the same stage as decode, so the latch being
  // checked has not been decoded yet.
  const ControlSignals s = control_.decode(in.instr);
  const uint8_t rs = decodeRs(in.instr);
  const uint8_t rt = decodeRt(in.instr);
  const uint8_t rd = decodeRd(in.instr);

  bool readsA = false;
  bool readsB = false;
  uint8_t regA = 0;
  uint8_t regB = 0;
  bool readsFlags = false;
  bool usesSp = false;
  const bool usesMem = s.memRead || s.memWrite || s.push || s.pop || s.ret;

  switch (s.opcode) {
    case Opcode::FADD:
    case Opcode::FSUB:
    case Opcode::FMUL:
    case Opcode::FDIV:
    case Opcode::ADD:
    case Opcode::SUB:
    case Opcode::MUL:
    case Opcode::DIV:
    case Opcode::AND:
    case Opcode::OR:
    case Opcode::XOR:
    case Opcode::SHL:
    case Opcode::SHR:
      readsA = true;
      regA = rs;
      readsB = true;
      regB = rt;
      break;
    case Opcode::NOT:
    case Opcode::MOV:
    case Opcode::ADDI:
    case Opcode::LW:
      readsA = true;
      regA = rs;
      break;
    case Opcode::CMP:
      readsA = true;
      regA = rs;
      readsB = true;
      regB = rt;
      break;
    case Opcode::SW:
      readsA = true;
      regA = rs;
      readsB = true;
      regB = rd;
      break;
    case Opcode::PUSH:
      readsA = true;
      regA = rd;
      usesSp = true;
      break;
    case Opcode::POP:
    case Opcode::RET:
    case Opcode::CALL:
      usesSp = true;
      break;
    case Opcode::BEQ:
    case Opcode::BNE:
    case Opcode::BLT:
    case Opcode::BGE:
      readsFlags = true;
      break;
    default:
      break;
  }

  for (int i = from + 1; i <= stages_ - 1; ++i) {
    const PipeLatch& p = ahead[i];
    if (!p.valid) {
      continue;
    }
    if (p.writesReg && ((readsA && p.destReg == regA) || (readsB && p.destReg == regB))) {
      if (!p.resultReady) {
        return true;
      }
    }
    if (readsFlags && p.writesFlags) {
      return true;
    }
    if (usesSp && p.writesSp) {
      return true;
    }
    // Keep memory accesses in order: a load or store must not overlap one that
    // is still in flight.
    if (usesMem) {
      const ControlSignals& ps = p.signals;
      if (ps.memRead || ps.memWrite || ps.push || ps.pop || ps.ret) {
        return true;
      }
    }
  }
  return false;
}

PipeLatch CyclePipeline::executeLatch(const PipeLatch& in) {
  PipeLatch out = in;
  const ControlSignals& s = in.signals;

  if (s.halt) {
    out.stop = true;
    out.stopIsHalt = true;
    return out;
  }

  const AluResult result = alu_.execute(s.aluOp, in.operandA, in.operandB);
  if (result.exception != ExceptionCode::NONE) {
    out.stop = true;
    out.stopException = result.exception;
    return out;
  }
  out.aluResult = result.value;
  out.flags = result.flags;
  out.resultValue = result.value;
  out.resultReady = !in.isLoad;

  if (s.push) {
    const ExceptionCode stack = safety_.checkStack(in.sp, true);
    if (stack != ExceptionCode::NONE) {
      out.stop = true;
      out.stopException = stack;
      return out;
    }
    out.memAddress = in.sp - 1;
    out.storeValue = s.opcode == Opcode::CALL ? in.pc + 1 : in.storeData;
    out.nextSp = in.sp - 1;
    out.writesSp = true;
    out.writesReg = false;
  } else if (s.pop) {
    const ExceptionCode stack = safety_.checkStack(in.sp, false);
    if (stack != ExceptionCode::NONE) {
      out.stop = true;
      out.stopException = stack;
      return out;
    }
    out.memAddress = in.sp;
    out.nextSp = in.sp + 1;
    out.writesSp = true;
  } else if (s.ret) {
    const ExceptionCode stack = safety_.checkStack(in.sp, false);
    if (stack != ExceptionCode::NONE) {
      out.stop = true;
      out.stopException = stack;
      return out;
    }
    out.target = cpu_.readMem(in.sp);
    out.nextSp = in.sp + 1;
    out.writesSp = true;
    out.redirect = true;
  } else if (s.memRead || s.memWrite) {
    const ExceptionCode memory = safety_.checkMem(result.value, s.memWrite);
    if (memory != ExceptionCode::NONE) {
      out.stop = true;
      out.stopException = memory;
      return out;
    }
    out.memAddress = result.value;
    out.storeValue = in.storeData;
  }

  if (s.jump || (s.branch && branchTaken(s.opcode, cpu_.flags()))) {
    out.target = in.pc + 1 + static_cast<uint32_t>(decodeOffset(in.instr));
    out.redirect = true;
  }
  return out;
}

// Only the 6 stage pipeline has a memory stage; the 4 stage does memory in WB.
PipeLatch CyclePipeline::memoryLatch(const PipeLatch& in) {
  PipeLatch out = in;
  if (in.signals.memWrite) {
    cpu_.writeMem(in.memAddress, in.storeValue);
  }
  if (in.isLoad) {
    out.loadValue = cpu_.readMem(in.memAddress);
    out.resultValue = out.loadValue;
    out.resultReady = true;
  }
  return out;
}

void CyclePipeline::writeBackLatch(const PipeLatch& in) {
  if (!in.valid) {
    return;
  }
  const ControlSignals& s = in.signals;

  if (stages_ == 4) {
    // The 4 stage pipeline folds the memory stage into write back.
    if (s.memWrite) {
      cpu_.writeMem(in.memAddress, in.storeValue);
    }
  }

  uint32_t value = in.resultValue;
  if (in.isLoad && stages_ == 4) {
    value = cpu_.readMem(in.memAddress);
  }

  if (s.regWrite) {
    cpu_.setReg(decodeRd(in.instr), value);
  }
  if (s.writeFlags) {
    cpu_.setFlags(in.flags);
  }
  if (in.writesSp) {
    cpu_.setSp(in.nextSp);
  }
  cpu_.setPc(in.pc + 1);
  ++retired_;
}

void CyclePipeline::cycle() {
  if (done_) {
    return;
  }
  ++cycles_;

  const int orIdx = operandStage();
  const int exIdx = executeStage();
  const int wbIdx = writeBackStage();
  const bool hasMemory = memoryStage() >= 0;

  std::array<PipeLatch, kMaxStages> res{};

  // Write back first, so its commit is visible to the operand read below.
  // Latch i holds the output of stage i, so write back reads latch wbIdx - 1.
  if (lat_[wbIdx - 1].valid) {
    writeBackLatch(lat_[wbIdx - 1]);
  }
  res[wbIdx] = PipeLatch{};

  bool stall = false;
  bool redirecting = false;
  uint32_t redirectTarget = 0;
  int stopStage = -1;
  PipeLatch stopLatch;

  // Walk the older stages first so the operand read can see their results.
  for (int i = wbIdx - 1; i >= 1; --i) {
    if (i == orIdx && detectStall(lat_[i - 1], res, i)) {
      stall = true;
      break;
    }

    PipeLatch out;
    if (i == 1) {
      out = decodeLatch(lat_[i - 1]);
      if (stages_ == 4 && !out.stop) {
        // The 4 stage pipeline reads operands in the same stage as decode.
        out = operandLatch(out, res);
      }
    } else if (i == orIdx) {
      out = operandLatch(lat_[i - 1], res);
    } else if (i == exIdx) {
      out = executeLatch(lat_[i - 1]);
    } else if (hasMemory && i == memoryStage()) {
      out = memoryLatch(lat_[i - 1]);
    } else {
      out = lat_[i - 1];
    }

    res[i] = out;

    if (out.stop) {
      stopStage = i;
      stopLatch = out;
      break;
    }
    if (out.redirect) {
      // The branch resolved, everything fetched behind it is wrong, stop here.
      redirecting = true;
      redirectTarget = out.target;
      break;
    }
  }

  // Build the next latches.
  std::array<PipeLatch, kMaxStages> next{};
  bool wantFetch = false;

  if (stopStage >= 0) {
    // Kill the offending instruction and everything younger, drain the rest.
    for (int i = stopStage + 1; i <= wbIdx; ++i) {
      next[i] = res[i];
    }
    draining_ = true;
    stopPc_ = lat_[stopStage - 1].pc;
    stopHalt_ = stopLatch.stopIsHalt;
    stopException_ = stopLatch.stopException;
  } else if (redirecting) {
    ++flushes_;
    for (int i = exIdx + 1; i <= wbIdx; ++i) {
      next[i] = res[i];
    }
    next[exIdx] = res[exIdx];
    if (!draining_) {
      fetchPc_ = redirectTarget;
      wantFetch = true;
    }
  } else if (stall) {
    ++stalls_;
    for (int i = 0; i <= orIdx - 1; ++i) {
      next[i] = lat_[i];
    }
    for (int i = orIdx + 1; i <= wbIdx; ++i) {
      next[i] = res[i];
    }
  } else {
    for (int i = 1; i <= wbIdx; ++i) {
      next[i] = res[i];
    }
    wantFetch = !draining_;
  }

  if (wantFetch) {
    if (fetchPc_ > kCodeLimit) {
      // PC has walked off the end of the code region.
      draining_ = true;
      stopPc_ = fetchPc_;
      stopHalt_ = false;
      stopException_ = ExceptionCode::PC_OUT_OF_RANGE;
    } else {
      next[0] = fetchLatch();
    }
  }

  lat_ = next;

  if (draining_ && empty()) {
    done_ = true;
    cpu_.setPc(stopPc_);
    if (stopHalt_) {
      // HALT never reaches write back, but it does complete, so count it so
      // the retired total matches the sequential reference.
      ++retired_;
      cpu_.setHalted(true);
    } else if (stopException_ != ExceptionCode::NONE) {
      cpu_.setException(stopException_);
    }
  }
}

}  // namespace risc201
