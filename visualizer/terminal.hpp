#pragma once

// Thin wrapper over the terminal: raw mode, alternate screen and key reads.
// When stdin or stdout is not a terminal it becomes a no-op, so the visualizer
// can be piped or run headless.

#include <termios.h>

#include <string>

namespace risc201 {

class Terminal {
 public:
  Terminal();
  ~Terminal();

  Terminal(const Terminal&) = delete;
  Terminal& operator=(const Terminal&) = delete;

  bool interactive() const { return interactive_; }
  int width() const { return width_; }
  int height() const { return height_; }

  // Draws a full frame, replacing what was on screen.
  void draw(const std::string& frame) const;

  // Waits up to timeoutMs for a key. Returns -1 when nothing was pressed.
  int readKey(int timeoutMs) const;

 private:
  bool interactive_ = false;
  termios original_{};
  int width_ = 100;
  int height_ = 40;
};

}  // namespace risc201
