# Stack preprocessor

`assembler/preprocessor.cpp` runs before the two assembler passes. It expands
macros, which is how the assembler provides preprocessor support for the
full-descending `PUSH`, `POP`, `CALL` and `RET` stack operations.

The interface is one function:

```cpp
std::vector<SourceLine> preprocess(const std::vector<SourceLine>& input);
```

`SourceLine` carries the text and its 1-based source line, so every error after
expansion still points at a real source line.

## Why a macro preprocessor

The ISA has native stack instructions, but a program often needs a *sequence* of
them: save a group of registers, restore them in reverse order, or allocate a
stack frame. The preprocessor lets those sequences be named once and expanded
into the flat instruction stream the assembler encodes. For a full-descending
stack the invariant is simple: whatever is pushed last is popped first, so a
`SAVE a, b` macro pushes `a` then `b`, and the matching `RESTORE` pops `b` then
`a`.

`tests/programs/stack_macros.s` is a runnable example; its hex output is in
`tests/programs/stack_macros.hex`.

## Definition syntax

```asm
.macro SAVE a, b
  PUSH \a
  PUSH \b
.endmacro
```

- `.macro NAME` defines a zero-parameter macro.
- `.macro NAME a, b` and `.macro NAME(a, b)` both define a two-parameter macro.
- The body ends at `.endmacro`. Nested definitions are rejected.
- Macros must be defined before use; a forward reference is just an unknown
  mnemonic and is reported by the assembler.

## Invocation syntax

```asm
SAVE R1, R2
SAVE(R1, R2)
```

- A line's first identifier is checked against the macro table. If it matches,
  the rest of the line is split on commas into arguments.
- The argument count must equal the parameter count.
- A label such as `SAVE:` is not treated as an invocation, so a label may share
  a macro's spelling.

## Parameter substitution

| Form | Meaning |
|---|---|
| `\name` | the argument bound to parameter `name` |
| `\1`, `\2`, ... | the first, second, ... argument |

Unknown names, out-of-range positions and a wrong argument count all throw a
`std::runtime_error` with the invocation's line number. Expansion is recursive
with a depth limit of 64; a self-recursive macro is a line-numbered error.

## Limitations

- Comments are stripped only for directive and invocation detection. A backslash
  sequence inside an inline comment in a macro body is still treated as a
  parameter reference, so avoid `\` in macro comments.
- Macros cannot generate unique labels. A macro that expands a label twice trips
  the assembler's duplicate-label check.
- There are no `.define` constants or conditional directives; this is purely
  textual macro expansion.

## Tests

`tests/test_assembler.cpp` covers:

- a `SAVE`/`RESTORE` pair expanding to the expected `PUSH`/`POP` words,
- the `\name` and positional `\1` forms,
- the parenthesised definition and invocation forms,
- wrong argument count, missing `.endmacro` and unknown parameter errors.
