#include "visualizer/terminal.hpp"

#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <iostream>

namespace risc201 {

Terminal::Terminal() {
  interactive_ = isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
  if (!interactive_) {
    return;
  }

  tcgetattr(STDIN_FILENO, &original_);

  // No line buffering and no echo, so a key press is seen straight away.
  termios raw = original_;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  // Alternate screen, hidden cursor, clear.
  std::cout << "\033[?1049h\033[?25l\033[2J\033[H" << std::flush;

  winsize size;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
    if (size.ws_col > 40) {
      width_ = size.ws_col;
    }
    if (size.ws_row > 12) {
      height_ = size.ws_row;
    }
  }
}

Terminal::~Terminal() {
  if (!interactive_) {
    return;
  }
  std::cout << "\033[?25h\033[?1049l" << std::flush;
  tcsetattr(STDIN_FILENO, TCSANOW, &original_);
}

void Terminal::draw(const std::string& frame) const {
  if (!interactive_) {
    std::cout << frame << std::flush;
    return;
  }
  // Home, frame, erase to end of screen.
  std::cout << "\033[H" << frame << "\033[J" << std::flush;
}

int Terminal::readKey(int timeoutMs) const {
  if (!interactive_) {
    return -1;
  }

  fd_set set;
  FD_ZERO(&set);
  FD_SET(STDIN_FILENO, &set);

  timeval timeout;
  timeout.tv_sec = timeoutMs / 1000;
  timeout.tv_usec = (timeoutMs % 1000) * 1000;

  const int ready = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout);
  if (ready <= 0) {
    return -1;
  }

  unsigned char key = 0;
  if (read(STDIN_FILENO, &key, 1) != 1) {
    return -1;
  }
  return key;
}

}  // namespace risc201
