#include "assembler/preprocessor.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace risc201 {

namespace {

constexpr int kMaxExpansionDepth = 64;

struct Macro {
  std::vector<std::string> parameters;
  std::vector<SourceLine> body;
  int definedAt = 0;
};

std::string trim(const std::string& text) {
  const std::size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

std::string stripComment(const std::string& text) {
  const std::size_t comment = text.find_first_of(";#");
  return comment == std::string::npos ? text : text.substr(0, comment);
}

bool isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::vector<std::string> splitArguments(const std::string& text, int line) {
  std::vector<std::string> arguments;
  if (trim(text).empty()) {
    return arguments;
  }

  std::string current;
  for (const char c : text) {
    if (c == ',') {
      const std::string argument = trim(current);
      if (argument.empty()) {
        throw std::runtime_error("Line " + std::to_string(line) +
                                 ": empty macro argument between commas");
      }
      arguments.push_back(argument);
      current.clear();
    } else {
      current += c;
    }
  }

  const std::string argument = trim(current);
  if (argument.empty()) {
    throw std::runtime_error("Line " + std::to_string(line) +
                             ": empty macro argument after the last comma");
  }
  arguments.push_back(argument);
  return arguments;
}

// A parameter list may be written as "a, b" or "(a, b)".
std::vector<std::string> parseParameters(const std::string& text, int line) {
  std::string list = trim(text);
  if (!list.empty() && list.front() == '(') {
    if (list.back() != ')') {
      throw std::runtime_error("Line " + std::to_string(line) +
                               ": unterminated macro parameter list");
    }
    list = trim(list.substr(1, list.size() - 2));
  }
  return splitArguments(list, line);
}

std::string substitute(const std::string& body, const Macro& macro,
                       const std::vector<std::string>& arguments, int line) {
  std::string out;
  out.reserve(body.size());

  for (std::size_t i = 0; i < body.size(); ++i) {
    if (body[i] != '\\') {
      out += body[i];
      continue;
    }

    if (i + 1 >= body.size()) {
      out += '\\';
      continue;
    }

    std::size_t next = i + 1;
    std::string key;
    if (std::isdigit(static_cast<unsigned char>(body[next])) != 0) {
      while (next < body.size() &&
             std::isdigit(static_cast<unsigned char>(body[next])) != 0) {
        key += body[next];
        ++next;
      }
      const std::size_t index = static_cast<std::size_t>(std::stoul(key));
      if (index == 0 || index > arguments.size()) {
        throw std::runtime_error("Line " + std::to_string(line) +
                                 ": macro positional parameter \\" + key +
                                 " has no matching argument");
      }
      out += arguments[index - 1];
    } else if (isIdentifierStart(body[next])) {
      while (next < body.size() && isIdentifierChar(body[next])) {
        key += body[next];
        ++next;
      }
      std::size_t index = macro.parameters.size();
      for (std::size_t p = 0; p < macro.parameters.size(); ++p) {
        if (macro.parameters[p] == key) {
          index = p;
          break;
        }
      }
      if (index == macro.parameters.size()) {
        throw std::runtime_error("Line " + std::to_string(line) +
                                 ": unknown macro parameter \\" + key);
      }
      out += arguments[index];
    } else {
      out += '\\';
      continue;
    }

    i = next - 1;
  }

  return out;
}

void expand(const SourceLine& line, const std::unordered_map<std::string, Macro>& macros,
            std::vector<SourceLine>& out, int depth);

void expandInvocation(const std::string& name, const std::string& argumentText,
                      int lineNumber, const Macro& macro,
                      const std::unordered_map<std::string, Macro>& macros,
                      std::vector<SourceLine>& out, int depth) {
  if (depth >= kMaxExpansionDepth) {
    throw std::runtime_error("Line " + std::to_string(lineNumber) +
                             ": macro expansion nested too deeply (recursive macro '" +
                             name + "')");
  }

  const std::vector<std::string> arguments = splitArguments(argumentText, lineNumber);
  if (arguments.size() != macro.parameters.size()) {
    throw std::runtime_error("Line " + std::to_string(lineNumber) + ": macro '" + name +
                             "' expects " + std::to_string(macro.parameters.size()) +
                             " argument(s), got " + std::to_string(arguments.size()));
  }

  for (const SourceLine& bodyLine : macro.body) {
    const SourceLine expanded{substitute(bodyLine.text, macro, arguments, lineNumber),
                              lineNumber};
    expand(expanded, macros, out, depth + 1);
  }
}

void expand(const SourceLine& line, const std::unordered_map<std::string, Macro>& macros,
            std::vector<SourceLine>& out, int depth) {
  const std::string clean = trim(stripComment(line.text));
  if (clean.empty()) {
    out.push_back(line);
    return;
  }

  std::size_t end = 0;
  while (end < clean.size() && isIdentifierChar(clean[end])) {
    ++end;
  }
  const std::string name = clean.substr(0, end);

  // A label with the same spelling as a macro is still a label, not an
  // invocation.
  if (end < clean.size() && clean[end] == ':') {
    out.push_back(line);
    return;
  }

  const auto it = macros.find(name);
  if (it == macros.end()) {
    out.push_back(line);
    return;
  }

  std::string argumentText = trim(clean.substr(end));
  if (!argumentText.empty() && argumentText.front() == '(') {
    if (argumentText.back() != ')') {
      throw std::runtime_error("Line " + std::to_string(line.line) +
                               ": unterminated macro argument list");
    }
    argumentText = trim(argumentText.substr(1, argumentText.size() - 2));
  }

  expandInvocation(name, argumentText, line.line, it->second, macros, out, depth);
}

} // namespace

std::vector<SourceLine> preprocess(const std::vector<SourceLine>& input) {
  std::unordered_map<std::string, Macro> macros;
  std::vector<SourceLine> out;

  Macro current;
  std::string currentName;
  bool insideMacro = false;

  for (const SourceLine& line : input) {
    const std::string clean = trim(stripComment(line.text));

    const bool macroDirective =
        clean.rfind(".macro", 0) == 0 &&
        (clean.size() == 6 || std::isspace(static_cast<unsigned char>(clean[6])) != 0 ||
         clean[6] == '(');
    if (macroDirective) {
      if (insideMacro) {
        throw std::runtime_error("Line " + std::to_string(line.line) +
                                 ": nested .macro definitions are not supported");
      }

      std::string rest = trim(clean.substr(6));
      std::size_t nameEnd = 0;
      while (nameEnd < rest.size() && isIdentifierChar(rest[nameEnd])) {
        ++nameEnd;
      }
      const std::string name = rest.substr(0, nameEnd);
      if (name.empty() || !isIdentifierStart(name[0])) {
        throw std::runtime_error("Line " + std::to_string(line.line) +
                                 ": .macro needs a name");
      }
      if (macros.find(name) != macros.end()) {
        throw std::runtime_error("Line " + std::to_string(line.line) +
                                 ": duplicate macro '" + name + "'");
      }

      current = Macro{};
      current.definedAt = line.line;
      current.parameters = parseParameters(rest.substr(nameEnd), line.line);
      currentName = name;
      insideMacro = true;
      continue;
    }

    if (clean == ".endmacro") {
      if (!insideMacro) {
        throw std::runtime_error("Line " + std::to_string(line.line) +
                                 ": .endmacro without a matching .macro");
      }
      macros[currentName] = std::move(current);
      insideMacro = false;
      currentName.clear();
      continue;
    }

    if (insideMacro) {
      current.body.push_back(line);
      continue;
    }

    expand(line, macros, out, 0);
  }

  if (insideMacro) {
    throw std::runtime_error("Line " + std::to_string(current.definedAt) +
                             ": macro '" + currentName +
                             "' is missing its .endmacro");
  }

  return out;
}

} // namespace risc201
