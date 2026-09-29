#include <cassert>
#include <iostream>
#include "control_unit/control_unit.hpp"
#include "isa_spec/isa201.h"

using namespace risc201;

int main() {
  MicroprogrammedControl control;

  // test our decode using RISC201 LW instruction (0x4A200004)
  // LW r8, 4(r8) -> opcode 0x12
  ControlSignals dec = control.decode(0x4A200004);
  
  assert(dec.opcode == Opcode::LW);
  assert(dec.memRead == true);
  assert(dec.regWrite == true);
  assert(dec.aluSrc == AluSrc::Immediate);
  assert(dec.resultSrc == ResultSrc::Memory);
  assert(dec.illegal == false);
  
  // test an illegal opcode
  ControlSignals bad = control.decode(0xFFFFFFFF);
  assert(bad.illegal == true);
  
  std::cout << "control unit tests passed!\n";
  return 0;
}