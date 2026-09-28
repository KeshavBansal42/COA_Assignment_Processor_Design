#include "assembler.hpp"

#include <cctype>
#include <string>

namespace risc201 {

namespace {

[[maybe_unused]] std::string stripCommentsAndTrim(const std::string& line) {
  const size_t comment = line.find_first_of(";#");
  const std::string clean = (comment == std::string::npos) ? line : line.substr(0, comment);
  const size_t first = clean.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = clean.find_last_not_of(" \t\r\n");
  return clean.substr(first, last - first + 1);
}

[[maybe_unused]] bool isValidIdentifier(const std::string& id) {
  if (id.empty() || (!std::isalpha(static_cast<unsigned char>(id[0])) && id[0] != '_')) {
    return false;
  }
  for (size_t i = 1; i < id.size(); ++i) {
    if (!std::isalnum(static_cast<unsigned char>(id[i])) && id[i] != '_') {
      return false;
    }
  }
  return true;
}

}  // namespace

void assemble(const std::string& /*path*/, std::vector<uint32_t>& /*outProgram*/) {
}

void assemble(const std::string& /*path*/) {
}

}  // namespace risc201