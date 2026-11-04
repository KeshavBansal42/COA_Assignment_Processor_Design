#include "visualizer/view.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace risc201 {

namespace {

// How many stack words and code lines the panels show.
constexpr uint32_t kStackDepth = 8;
constexpr std::size_t kCodeWindow = 14;

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

  for (std::size_t i = 0; i < kRegisterCount; ++i) {
    const uint32_t value = cpu.reg(static_cast<uint8_t>(i));
    std::string line = " r" + pad(decimal(static_cast<uint32_t>(i)), 2) + " " + hexWord(value);
    if (previous != nullptr && i < previous->regs.size() && previous->regs[i] != value) {
      line += "  <- was " + hexWord(previous->regs[i]);
    }
    lines.push_back(line);
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
  std::ostringstream header;
  header << " ";
  for (const std::string& name : names) {
    header << pad(name, 14);
  }
  lines.push_back(header.str());

  std::ostringstream row;
  row << " ";
  for (int stage = pipeline.stages() - 1; stage >= 0; --stage) {
    const PipeLatch& latch = pipeline.latch(stage);
    std::string cell = "-";
    if (latch.valid) {
      cell = hexAddr(latch.pc) + " " + mnemonicOf(decodeOpcode(latch.instr));
    }
    row << pad(cell, 14);
  }
  lines.push_back(row.str());

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

  std::ostringstream out;
  out << "RISC201 visualizer   " << options.program << "\n";
  out << "pipeline " << pipelineModeName(cpu.pipelineMode()) << "   microcode "
      << (cpu.controlMode() == ControlMode::Vertical ? "vertical" : "horizontal")
      << "   stack " << (cpu.dynamicStack() ? "dynamic" : "fixed") << "   cycles "
      << cpu.cycles() << "   instructions " << cpu.instructions() << "\n";

  const std::size_t width = options.width > 60 ? static_cast<std::size_t>(options.width) - 2 : 88;
  out << std::string(width, '-') << "\n";

  const std::size_t leftWidth = width * 5 / 12;
  out << columns(codePanel(options, current), leftWidth, machinePanel(cpu, previous));
  out << std::string(width, '-') << "\n";

  for (const std::string& line : pipelinePanel(cpu)) {
    out << line << "\n";
  }
  out << std::string(width, '-') << "\n";
  for (const std::string& line : stackPanel(cpu, previous)) {
    out << line << "\n";
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
