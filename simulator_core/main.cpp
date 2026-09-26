#include <iostream>
#include <string>

#include "simulator_core/cli.hpp"
#include "simulator_core/cpu.hpp"

int main(int argc, char** argv) {
  risc201::Cpu cpu;
  risc201::Cli cli(cpu, std::cout);

  std::cout << "RISC201 simulator, type help for commands\n";

  if (argc > 1) {
    cli.execute(std::string("load ") + argv[1]);
  }

  std::string line;
  while (true) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }
    if (!cli.execute(line)) {
      break;
    }
  }

  return 0;
}
