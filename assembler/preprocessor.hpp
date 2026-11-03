#pragma once

// Stack preprocessor for RISC201 assembly.
//
// Expands .macro / .endmacro definitions before the assembler's two passes run.
// The macros are how the full-descending stack operations are expressed: a
// program can group PUSH, POP, CALL and RET into named stack sequences and the
// preprocessor turns every invocation into the flat instruction stream the
// assembler consumes.
//
// A definition looks like
//
//     .macro SAVE a, b
//       PUSH \a
//       PUSH \b
//     .endmacro
//
// and an invocation is the macro name followed by comma separated arguments:
//
//     SAVE R1, R2
//
// Parameters are substituted with \name or the positional \1, \2, ... form.
// Macros must be defined before they are used and cannot be nested.

#include <string>
#include <vector>

namespace risc201 {

// One source line together with its 1-based line number, used for diagnostics
// after macro expansion changes where a line appears in the instruction stream.
struct SourceLine {
  std::string text;
  int line = 0;
};

// Expands every macro invocation and drops the definition directives.
// Throws std::runtime_error with a line number on a malformed definition,
// an unknown parameter or a recursive expansion.
std::vector<SourceLine> preprocess(const std::vector<SourceLine>& input);

} // namespace risc201
