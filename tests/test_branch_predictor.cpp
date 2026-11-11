#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"

using namespace risc201;

std::vector<uint32_t> loadLoop() {
  std::ifstream input(std::string(RISC201_PROGRAMS_DIR) + "/loop.hex");
  assert(input);
  std::vector<uint32_t> words;
  std::string line;
  while (std::getline(input, line)) {
    line = line.substr(0, line.find_first_of("#;"));
    std::istringstream in(line);
    std::string token;
    while (in >> token) words.push_back(static_cast<uint32_t>(std::stoul(token, nullptr, 16)));
  }
  return words;
}

void runBounded(Cpu& cpu) {
  for (int i = 0; i < 10000 && !cpu.halted() && cpu.exception() == ExceptionCode::NONE; ++i)
    cpu.step();
  assert(cpu.halted() || cpu.exception() != ExceptionCode::NONE);
}

void sameState(const Cpu& a, const Cpu& b) {
  for (uint8_t i = 0; i < kRegisterCount; ++i) assert(a.reg(i) == b.reg(i));
  for (uint32_t i = 0; i < kMemoryWords; ++i) assert(a.readMem(i) == b.readMem(i));
  if (a.pc() != b.pc() || a.sp() != b.sp() || !(a.flags() == b.flags()))
    std::cerr << "state pc " << a.pc() << " vs " << b.pc() << " sp " << a.sp() << " vs " << b.sp() << "\n";
  assert(a.pc() == b.pc() && a.sp() == b.sp() && a.flags() == b.flags());
  assert(a.exception() == b.exception() && a.halted() == b.halted());
  assert(a.instructions() == b.instructions());
}

int main() {
  const auto loop = loadLoop();
  Cpu reference;
  reference.load(loop);
  runBounded(reference);
  assert(reference.reg(3) == 55);

  for (PipelineMode mode : {PipelineMode::FourStage, PipelineMode::SixStage}) {
    Cpu dynamic;
    dynamic.load(loop);
    dynamic.setPipelineMode(mode);
    runBounded(dynamic);
    sameState(reference, dynamic);
    // The branch is taken nine times, then not taken: cold miss and exit miss.
    assert(dynamic.branchPredictions() == 10 && dynamic.branchMispredictions() == 2);
    const auto dynamicCycles = dynamic.cycles();
    dynamic.reset();
    runBounded(dynamic);
    assert(dynamic.branchPredictions() == 10 && dynamic.branchMispredictions() == 2);
    assert(dynamic.cycles() == dynamicCycles);  // reset clears predictor history

    Cpu baseline;
    baseline.setBranchPrediction(false);
    baseline.load(loop);
    baseline.setPipelineMode(mode);
    runBounded(baseline);
    sameState(reference, baseline);
    assert(baseline.branchPredictions() == 10 && baseline.branchMispredictions() == 9);
    assert(dynamicCycles < baseline.cycles());

    // A wrong-path branch and its store must not resolve or update memory.
    std::vector<uint32_t> wrongPath = {
      encodeI(Opcode::LDI, 1, 0, 7), encodeI(Opcode::LDI, 2, 0, 0x800),
      encodeR(Opcode::CMP, 0, 1, 1), encodeJ(Opcode::BEQ, 2),
      encodeJ(Opcode::BEQ, 0), encodeI(Opcode::SW, 1, 2, 0),
      encodeI(Opcode::LDI, 3, 0, 42), encodeOpcode(Opcode::HALT)};
    Cpu expected;
    expected.load(wrongPath);
    runBounded(expected);
    Cpu actual;
    actual.load(wrongPath);
    actual.setPipelineMode(mode);
    runBounded(actual);
    sameState(expected, actual);
    assert(actual.reg(3) == 42 && actual.readMem(kDataBase) == 0);
    assert(actual.branchPredictions() == 1 && actual.branchMispredictions() == 1);

    // A younger illegal instruction is also squashed by the older redirect.
    wrongPath[4] = 0xFC000000u;
    expected.load(wrongPath);
    runBounded(expected);
    actual.load(wrongPath);
    runBounded(actual);
    sameState(expected, actual);
    assert(actual.halted() && actual.exception() == ExceptionCode::NONE);

    // Fetch past the code limit is speculative while an older jump resolves.
    std::vector<uint32_t> edge(kCodeLimit + 1, encodeOpcode(Opcode::NOP));
    edge[0] = encodeJ(Opcode::JMP, static_cast<int32_t>(kCodeLimit) - 1);
    edge[1] = encodeOpcode(Opcode::HALT);
    edge[kCodeLimit] = encodeJ(Opcode::JMP, -static_cast<int32_t>(kCodeLimit));
    Cpu edgeReference;
    edgeReference.load(edge);
    runBounded(edgeReference);
    Cpu edgePipeline;
    edgePipeline.load(edge);
    edgePipeline.setPipelineMode(mode);
    runBounded(edgePipeline);
    sameState(edgeReference, edgePipeline);
    assert(edgePipeline.halted() && edgePipeline.exception() == ExceptionCode::NONE);

    // An actual out-of-range fetch must still fault precisely.
    edge[kCodeLimit] = encodeOpcode(Opcode::NOP);
    edgeReference.load(edge);
    runBounded(edgeReference);
    edgePipeline.load(edge);
    runBounded(edgePipeline);
    sameState(edgeReference, edgePipeline);
    assert(edgePipeline.exception() == ExceptionCode::PC_OUT_OF_RANGE);
    assert(edgePipeline.pc() == kCodeLimit + 1);
  }
}
