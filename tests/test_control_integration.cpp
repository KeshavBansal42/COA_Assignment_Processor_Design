#include <cassert>
#include <iostream>
#include "control_unit/control_unit.hpp"
#include "simulator_core/simple_safety.hpp"

using namespace risc201;

int main() {
  SimpleSafety safety;
  MicroprogrammedControl control;
  
  // mock instruction: LW r1, 0x1000(r2) 
  // lets say r2 is 0x0000, so target address is 0x1000 (which is out of bounds)
  uint32_t mockLwInstr = encodeI(Opcode::LW, 1, 2, 0x1000);
  
  ControlSignals dec = control.decode(mockLwInstr);
  
  // mock cpu loop checking if we need to read memory
  if (dec.memRead) {
    uint32_t immediate = decodeImmediate(mockLwInstr);
    uint32_t targetAddr = 0 + immediate; // mock ALU addition
    
    // assert that Ayush's system catches the bad read before we proceed
    ExceptionCode err = safety.checkMem(targetAddr, false);
    assert(err == ExceptionCode::INVALID_MEM_ACCESS);
  } else {
    assert(false); // shouldnt reach here since LW sets MemRead true
  }

  std::cout << "control integration tests passed!\n";
  return 0;
}