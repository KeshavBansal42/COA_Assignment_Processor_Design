#include "visualizer/view.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace risc201 {

namespace {

// How many stack words and code lines the panels show.
constexpr uint32_t kStackDepth = 4;
constexpr std::size_t kCodeWindow = 8;

std::string hexWord(uint32_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
  return out.str();
}

std::string hexAddr(uint32_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value;
  return out.str();
}

std::string decimal(uint32_t value) {
  return std::to_string(value);
}

std::string pad(const std::string& text, std::size_t width) {
  if (text.size() >= width) {
    return text.substr(0, width);
  }
  return text + std::string(width - text.size(), ' ');
}

// Places two blocks of text side by side.
std::string columns(const std::vector<std::string>& left, std::size_t leftWidth,
                    const std::vector<std::string>& right) {
  std::ostringstream out;
  const std::size_t rows = std::max(left.size(), right.size());
  for (std::size_t i = 0; i < rows; ++i) {
    const std::string l = i < left.size() ? left[i] : std::string();
    const std::string r = i < right.size() ? right[i] : std::string();
    out << pad(l, leftWidth) << " | " << r << "\n";
  }
  return out.str();
}

// The instruction the machine is working on: the oldest one still in the
// pipeline, or the PC when the pipeline is sequential.
uint32_t currentInstruction(const Cpu& cpu) {
  if (cpu.pipelineMode() == PipelineMode::Sequential) {
    return cpu.pc();
  }
  const CyclePipeline& pipeline = cpu.cyclePipeline();
  for (int stage = pipeline.stages() - 1; stage >= 0; --stage) {
    if (pipeline.latch(stage).valid) {
      return pipeline.latch(stage).pc;
    }
  }
  return cpu.pc();
}

std::vector<std::string> codePanel(const ViewOptions& options, uint32_t current) {
  std::vector<std::string> lines;
  lines.push_back("CODE");

  const std::size_t count = options.code.size();
  if (count == 0) {
    lines.push_back("  (no program)");
    return lines;
  }

  std::size_t first = 0;
  if (count > kCodeWindow) {
    first = current > kCodeWindow / 2 ? current - kCodeWindow / 2 : 0;
    if (first + kCodeWindow > count) {
      first = count - kCodeWindow;
    }
  }

  for (std::size_t i = first; i < count && i < first + kCodeWindow; ++i) {
    const bool here = i == current;
    lines.push_back(std::string(here ? ">" : " ") + " " + hexAddr(static_cast<uint32_t>(i)) +
                    "  " + options.code[i]);
  }
  return lines;
}

std::vector<std::string> machinePanel(const Cpu& cpu, const MachineSnapshot* previous) {
  std::vector<std::string> lines;
  lines.push_back("REGISTERS");

  for (std::size_t i = 0; i < kRegisterCount / 2; ++i) {
    std::string line;
    std::string changes;
    for (std::size_t index : {i, i + kRegisterCount / 2}) {
      const uint32_t value = cpu.reg(static_cast<uint8_t>(index));
      std::string cell = "r" + pad(decimal(static_cast<uint32_t>(index)), 2) + " " + hexWord(value);
      if (previous != nullptr && index < previous->regs.size() && previous->regs[index] != value) {
        changes += " r" + decimal(static_cast<uint32_t>(index)) + " <- was " + hexWord(previous->regs[index]);
      }
      if (!line.empty()) line += "  ";
      line += cell;
    }
    lines.push_back(" " + line + changes);
  }

  const Flags flags = cpu.flags();
  lines.push_back(" PC " + hexWord(cpu.pc()) + "   SP " + hexWord(cpu.sp()));
  std::ostringstream flagLine;
  flagLine << " FLAGS Z" << flags.z << " N" << flags.n << " C" << flags.c << " V" << flags.v;
  lines.push_back(flagLine.str());

  if (cpu.exception() != ExceptionCode::NONE) {
    lines.push_back(" EXCEPTION " + toString(cpu.exception()));
  } else if (cpu.halted()) {
    lines.push_back(" HALTED");
  }
  return lines;
}

std::vector<std::string> pipelinePanel(const Cpu& cpu) {
  std::vector<std::string> lines;
  lines.push_back("PIPELINE");

  if (cpu.pipelineMode() == PipelineMode::Sequential) {
    lines.push_back(" sequential: one whole instruction per step");
    return lines;
  }

  const CyclePipeline& pipeline = cpu.cyclePipeline();
  const std::vector<std::string> names = pipeline.stageNames();
  for (int group = 0; group < pipeline.stages(); group += 3) {
    std::ostringstream row;
    row << " ";
    for (int offset = 0; offset < 3 && group + offset < pipeline.stages(); ++offset) {
      const int position = group + offset;
      const PipeLatch& latch = pipeline.latch(pipeline.stages() - 1 - position);
      std::string cell = names[position] + ":";
      if (latch.valid) cell += hexAddr(latch.pc) + " " + mnemonicOf(decodeOpcode(latch.instr));
      else cell += "-";
      row << pad(cell, 16);
    }
    lines.push_back(row.str());
  }

  std::ostringstream counters;
  counters << " stalls " << pipeline.stalls() << "  flushes " << pipeline.flushes()
           << "  forwards " << pipeline.forwards();
  lines.push_back(counters.str());
  return lines;
}

std::vector<std::string> stackPanel(const Cpu& cpu, const MachineSnapshot* previous) {
  std::vector<std::string> lines;
  lines.push_back("STACK");

  const MemorySafetyImpl& safety = cpu.memorySafety();
  const bool dynamic = safety.isDynamicStackEnabled();
  const uint32_t floor = safety.heapTop();
  const uint32_t sp = cpu.sp();

  lines.push_back(" SP = " + hexWord(sp));
  lines.push_back(std::string(" mode ") + (dynamic ? "dynamic" : "fixed") + "   stack floor " +
                  hexAddr(floor) + (dynamic ? " (heap top)" : ""));

  if (sp < floor || sp > kInitialSp) {
    lines.push_back(" (stack pointer outside stack region)");
    return lines;
  }

  lines.push_back(" depth " + decimal(kInitialSp - sp) + " words   capacity " +
                  decimal(sp - floor) + " words");
  if (dynamic) {
    lines.push_back(" heap " + decimal(safety.heapSize()) + " words used");
  }

  std::size_t shown = 0;
  for (uint32_t i = 0; i < kStackDepth; ++i) {
    const uint32_t address = sp + i;
    if (address > kLastAddress) {
      break;
    }
    const uint32_t value = cpu.readMem(address);
    std::string line = " " + hexAddr(address) + "  " + hexWord(value);
    if (i == 0) {
      line += "  <-- top of stack";
    }
    if (previous != nullptr && address >= previous->stackLow &&
        address - previous->stackLow < previous->stack.size() &&
        previous->stack[address - previous->stackLow] != value) {
      line += "   (was " + hexWord(previous->stack[address - previous->stackLow]) + ")";
    }
    lines.push_back(line);
    ++shown;
  }
  if (shown == 0) {
    lines.push_back(" (stack empty)");
  }
  return lines;
}

// A wide presentation layout. Box content is clipped to its column, so long
// paths and change annotations cannot push the next panel off the screen.
std::string presentationFrame(const Cpu& cpu, const ViewOptions& options,
                              const MachineSnapshot* previous, uint32_t current) {
  const std::size_t width = std::min<std::size_t>(static_cast<std::size_t>(options.width) - 1, 150);
  const std::size_t inner = width - 4;
  const std::size_t left = inner * 30 / 100;
  const std::size_t middle = inner * 38 / 100;
  const std::size_t right = inner - left - middle;
  const auto rule = [&] { return "+" + std::string(left, '-') + "+" +
                                 std::string(middle, '-') + "+" + std::string(right, '-') + "+\n"; };
  auto code = codePanel(options, current);
  auto machine = machinePanel(cpu, previous);
  auto stack = stackPanel(cpu, previous);
  // Keep change indicators legible even in the smallest wide layout.
  for (auto& line : machine) {
    const auto change = line.find(" <- was ");
    if (change != std::string::npos) {
      const auto label = line.rfind(" r", change);
      if (label != std::string::npos) line = line.substr(0, label) + "  * changed";
    }
  }
  for (auto& line : stack) {
    const auto change = line.find("   (was ");
    if (change != std::string::npos) line = line.substr(0, change) + " *";
    const auto top = line.find("  <-- top of stack");
    if (top != std::string::npos) line.replace(top, 18, "  <- top");
  }
  code[0] = "  PROGRAM / INSTRUCTIONS";
  machine[0] = "  REGISTER FILE / MACHINE STATE";
  stack[0] = "  STACK / MEMORY";
  std::ostringstream out;
  out << "RISC201 visualizer  /  PROCESSOR DASHBOARD\n";
  out << "  " << options.program << "   |   pipeline " << pipelineModeName(cpu.pipelineMode())
      << "   |   microcode " << (cpu.controlMode() == ControlMode::Vertical ? "vertical" : "horizontal")
      << "   |   stack " << (cpu.dynamicStack() ? "dynamic" : "fixed") << "\n";
  out << "  CYCLE " << cpu.cycles() << "     RETIRED " << cpu.instructions() << "     "
      << (cpu.exception() != ExceptionCode::NONE ? "EXCEPTION " + toString(cpu.exception()) :
          cpu.halted() ? "HALTED" : options.playing ? "RUNNING" : "PAUSED") << "\n";
  out << rule();
  const std::size_t rows = std::max({code.size(), machine.size(), stack.size(), std::size_t{12}});
  for (std::size_t i = 0; i < rows; ++i) {
    out << "|" << pad(i < code.size() ? code[i] : "", left)
        << "|" << pad(i < machine.size() ? machine[i] : "", middle)
        << "|" << pad(i < stack.size() ? stack[i] : "", right) << "|\n";
  }
  out << rule();
  out << "  PIPELINE / IN-FLIGHT INSTRUCTIONS\n";
  for (const auto& line : pipelinePanel(cpu)) {
    if (line != "PIPELINE") out << line << "\n";
  }
  out << std::string(width, '-') << "\n";
  out << "  [SPACE] play / pause    [S] instruction    [C] cycle    [R] reset    [+/-] speed    [Q] quit\n";
  out << "  " << (options.playing ? "[playing]" : "[paused]") << "  " << options.speedMs << "ms";
  if (!options.status.empty()) out << "    " << options.status;
  out << "\n";
  return out.str();
}

}  // namespace

MachineSnapshot snapshot(const Cpu& cpu) {
  MachineSnapshot snap;
  for (std::size_t i = 0; i < kRegisterCount; ++i) {
    snap.regs.push_back(cpu.reg(static_cast<uint8_t>(i)));
  }
  snap.pc = cpu.pc();
  snap.sp = cpu.sp();
  snap.flags = cpu.flags();
  // Capture the stack window plus the words just below SP, so a push into a
  // previously unseen address can still be highlighted on the next frame.
  const uint32_t sp = cpu.sp();
  snap.stackLow = sp > kStackDepth ? sp - kStackDepth : 0;
  const uint32_t high = std::min<uint32_t>(kLastAddress, sp + kStackDepth);
  for (uint32_t address = snap.stackLow; address <= high; ++address) {
    snap.stack.push_back(cpu.readMem(address));
  }
  return snap;
}

std::string renderFrame(const Cpu& cpu, const ViewOptions& options,
                        const MachineSnapshot* previous) {
  const uint32_t current = currentInstruction(cpu);
  if (options.width >= 110) return presentationFrame(cpu, options, previous, current);

  std::ostringstream out;
  out << "RISC201 visualizer   " << options.program << "\n";
  out << "pipeline " << pipelineModeName(cpu.pipelineMode()) << "   microcode "
      << (cpu.controlMode() == ControlMode::Vertical ? "vertical" : "horizontal")
      << "   stack " << (cpu.dynamicStack() ? "dynamic" : "fixed") << "   cycles "
      << cpu.cycles() << "   instructions " << cpu.instructions() << "\n";

  const std::size_t width = options.width >= 60 ? static_cast<std::size_t>(options.width) - 1 : 79;
  out << std::string(width, '-') << "\n";

  if (width >= 100) {
    const std::size_t leftWidth = 32;
    out << columns(codePanel(options, current), leftWidth, machinePanel(cpu, previous));
  } else {
    out << columns(codePanel(options, current), 27, machinePanel(cpu, previous));
  }
  out << std::string(width, '-') << "\n";
  if (width >= 95) {
    out << columns(pipelinePanel(cpu), 49, stackPanel(cpu, previous));
  } else {
    for (const std::string& line : pipelinePanel(cpu)) out << line << "\n";
    out << std::string(width, '-') << "\n";
    for (const std::string& line : stackPanel(cpu, previous)) out << line << "\n";
  }
  out << std::string(width, '-') << "\n";

  out << (options.playing ? "[playing]" : "[paused]") << "  speed " << options.speedMs << "ms"
      << "   space play/pause   s step   c cycle   r reset   +/- speed   q quit\n";
  if (!options.status.empty()) {
    out << options.status << "\n";
  }
  return out.str();
}

}  // namespace risc201
