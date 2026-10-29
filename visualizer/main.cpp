// Animated code and machine state visualizer.
//
// Usage: risc201-viz <program.s|program.hex> [--mode sequential|4stage|6stage]
//                    [--micro horizontal|vertical] [--speed ms] [--once]
//
// Shows the code the machine is working on, what the last step changed in the
// registers and the stack, and the state of the machine (PC, SP, flags, cycle
// count and the pipeline). Space plays and pauses, s steps one instruction,
// c steps one cycle, +/- change the speed, r resets and q quits.
//
// When stdout is not a terminal, or with --once, it runs the program to the end
// and prints the final frame once.

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "assembler/assembler.hpp"
#include "disassembler/disassembler.hpp"
#include "isa_spec/isa201.h"
#include "simulator_core/cpu.hpp"
#include "visualizer/terminal.hpp"
#include "visualizer/view.hpp"

using namespace risc201;

namespace {

bool endsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<uint32_t> loadHexFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("cannot open " + path);
  }

  std::vector<uint32_t> program;
  std::string line;
  while (std::getline(file, line)) {
    const std::size_t comment = line.find_first_of("#;");
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    std::istringstream words(line);
    std::string word;
    while (words >> word) {
      program.push_back(static_cast<uint32_t>(std::stoul(word, nullptr, 16)));
    }
  }
  if (program.empty()) {
    throw std::runtime_error(path + " has no words");
  }
  return program;
}

PipelineMode parseMode(const std::string& name, bool& ok) {
  ok = true;
  if (name == "sequential") return PipelineMode::Sequential;
  if (name == "4stage") return PipelineMode::FourStage;
  if (name == "6stage") return PipelineMode::SixStage;
  ok = false;
  return PipelineMode::SixStage;
}

ControlMode parseControl(const std::string& name, bool& ok) {
  ok = true;
  if (name == "horizontal") return ControlMode::Horizontal;
  if (name == "vertical") return ControlMode::Vertical;
  ok = false;
  return ControlMode::Horizontal;
}

void printUsage() {
  std::cerr << "usage: risc201-viz <program.s|program.hex> [--mode sequential|4stage|6stage]\n"
            << "                   [--micro horizontal|vertical] [--speed ms] [--once]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string program;
  PipelineMode mode = PipelineMode::SixStage;
  ControlMode control = ControlMode::Horizontal;
  int speedMs = 120;
  bool once = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--mode" && i + 1 < argc) {
      bool ok = false;
      mode = parseMode(argv[++i], ok);
      if (!ok) {
        printUsage();
        return 1;
      }
    } else if (arg == "--micro" && i + 1 < argc) {
      bool ok = false;
      control = parseControl(argv[++i], ok);
      if (!ok) {
        printUsage();
        return 1;
      }
    } else if (arg == "--speed" && i + 1 < argc) {
      speedMs = std::atoi(argv[++i]);
      if (speedMs < 10) {
        speedMs = 10;
      }
    } else if (arg == "--once") {
      once = true;
    } else if (arg.rfind("--", 0) == 0) {
      std::cerr << "unknown option " << arg << "\n";
      printUsage();
      return 1;
    } else {
      program = arg;
    }
  }

  if (program.empty()) {
    printUsage();
    return 1;
  }

  std::vector<uint32_t> words;
  try {
    if (endsWith(program, ".s") || endsWith(program, ".asm")) {
      assemble(program, words);
    } else {
      words = loadHexFile(program);
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }

  Cpu cpu;
  cpu.load(words);
  cpu.setControlMode(control);
  cpu.setPipelineMode(mode);

  ViewOptions options;
  options.program = program;
  for (const uint32_t word : words) {
    options.code.push_back(disassemble(word));
  }
  options.speedMs = speedMs;

  Terminal terminal;
  options.width = terminal.width();

  // Headless or one shot: run to the end and print the final frame.
  if (once || !terminal.interactive()) {
    cpu.run();
    options.status = cpu.halted() ? "halted" : "stopped: " + toString(cpu.exception());
    std::cout << renderFrame(cpu, options, nullptr);
    return 0;
  }

  bool playing = true;
  bool running = true;
  std::string status = "space to pause, q to quit";
  MachineSnapshot shown = snapshot(cpu);
  bool haveShown = false;

  while (running) {
    options.playing = playing;
    options.speedMs = speedMs;
    options.status = status;
    options.width = terminal.width();
    terminal.draw(renderFrame(cpu, options, haveShown ? &shown : nullptr));
    shown = snapshot(cpu);
    haveShown = true;

    const int key = terminal.readKey(playing ? speedMs : 200);
    if (key >= 0) {
      switch (key) {
        case ' ':
          playing = !playing;
          status = playing ? "playing" : "paused";
          continue;
        case 's': {
          // Step one whole instruction: run cycles until something retires.
          playing = false;
          const uint64_t before = cpu.instructions();
          int guard = 0;
          do {
            cpu.step();
          } while (cpu.instructions() == before && !cpu.halted() &&
                   cpu.exception() == ExceptionCode::NONE && ++guard < 64);
          status = "stepped one instruction";
          continue;
        }
        case 'c':
          playing = false;
          cpu.step();
          status = "stepped one cycle";
          continue;
        case 'r':
          cpu.reset();
          status = "reset";
          continue;
        case '+':
        case '=':
          speedMs = speedMs > 20 ? speedMs - 20 : 10;
          status = "faster";
          continue;
        case '-':
        case '_':
          speedMs += 20;
          status = "slower";
          continue;
        case 'q':
        case 'Q':
          running = false;
          continue;
        default:
          continue;
      }
    }

    if (cpu.halted() || cpu.exception() != ExceptionCode::NONE) {
      // Nothing left to run, keep showing the final state until a key or quit.
      continue;
    }

    if (playing) {
      cpu.step();
      status.clear();
    }
  }

  return 0;
}
