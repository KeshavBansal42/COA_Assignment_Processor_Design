# RISC201 assembly grammar

This is the grammar the shipped assembler (`assembler/assembler.cpp`) and the
stack preprocessor (`assembler/preprocessor.cpp`) actually accept.

Mnemonics and register names are **strict uppercase**. Labels and macro names
are case-sensitive identifiers. Every statement is one source line; there is no
statement separator.

```ebnf
program        = { line } ;
line           = blank | comment_line | statement ;
statement      = label | macro_definition | macro_call | instruction ;
blank          = newline ;
comment_line   = comment , newline ;

label          = identifier , ":" , newline ;       (* must be on its own line *)

macro_definition = ".macro" , identifier , [ parameters ] , newline ,
                   { line } ,
                   ".endmacro" , newline ;
parameters     = [ "(" ] , [ identifier , { "," , identifier } ] , [ ")" ] ;
macro_call     = identifier , [ "(" ] ,
                 [ argument , { "," , argument } ] ,
                 [ ")" ] , newline ;
argument       = text ;                              (* substituted verbatim *)

instruction    = opcode , [ operands ] , newline ;
operands       = r_operands | two_reg_operands | cmp_operands |
                 i_arith_operands | ldi_operands | mem_operands |
                 stack_operand | j_operand ;
r_operands     = register , "," , register , "," , register ;
two_reg_operands = register , "," , register ;
cmp_operands   = register , "," , register ;
i_arith_operands = register , "," , register , "," , immediate ;
ldi_operands   = register , "," , immediate ;
mem_operands   = register , "," , immediate , "(" , register , ")" ;
stack_operand  = register ;
j_operand      = identifier ;                        (* a label, not a number *)

opcode         = "NOP" | "HALT" | "ADD" | "SUB" | "MUL" | "DIV" |
                 "AND" | "OR" | "XOR" | "NOT" | "SHL" | "SHR" |
                 "MOV" | "CMP" | "ADDI" | "LDI" | "LW" | "SW" |
                 "PUSH" | "POP" | "RET" | "JMP" | "BEQ" | "BNE" |
                 "BLT" | "BGE" | "CALL" |
                 "FADD" | "FSUB" | "FMUL" | "FDIV" ;

register       = "R" , ( digit | "1" , ( "0" | "1" | "2" | "3" | "4" | "5" ) ) ;
immediate      = [ "-" ] , digits                     (* decimal, signed *)
               | "0x" , hex_digits                    (* hex, non-negative *) ;

identifier     = ( letter | "_" ) , { letter | digit | "_" } ;
comment        = ( ";" | "#" ) , { any_character_except_newline } ;

letter         = "A" | "B" | ... | "Z" | "a" | "b" | ... | "z" ;
digit          = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
hex_digits     = hex_digit , { hex_digit } ;
hex_digit      = digit | "a" | ... | "f" | "A" | ... | "F" ;
newline        = "\n" ;
```

## Notes the grammar cannot express

- A label is only recognised on a line of its own. `loop: ADD ...` is rejected;
  write the label on the previous line.
- `JMP`, `BEQ`, `BNE`, `BLT`, `BGE` and `CALL` take a label. A raw numeric PC
  offset is rejected; the assembler computes `target - (PC + 1)` itself.
- `LW` and `SW` require an explicit immediate, so `LW R1, (R2)` is rejected and
  must be written `LW R1, 0(R2)`.
- The assembler has no data directives (`.data`, `.word`, `.space`, ...); the
  output is an instruction stream only.
- The output `.hex` file is written beside the source with the same stem and
  holds one bare 8-digit hex word per line.

## Stack macros

Macros are the preprocessor support for the full-descending stack. A parameter
is substituted with `\name` or the positional `\1`, `\2`, ... form:

```asm
.macro SAVE a, b
  PUSH \a
  PUSH \b
.endmacro

.macro RESTORE a, b
  POP \b
  POP \a
.endmacro

SAVE R1, R2        ; expands to PUSH R1; PUSH R2
RESTORE R1, R2     ; expands to POP R2; POP R1
```

Rules:

- A macro must be defined before it is used, and definitions cannot nest.
- The invocation supplies comma separated arguments; the count must match the
  parameter count.
- Arguments and parameters may also be written with parentheses: `.macro
  RESTORE(a, b)` and `RESTORE(R1, R2)`.
- Expansion is recursive with a depth limit of 64. A macro that expands itself
  is reported as a line-numbered error.
- A label with the same spelling as a macro is still treated as a label.
- A macro that emits a label will produce a duplicate-label error if it is
  expanded more than once; label-generating macros are not supported.

See `docs/preprocessor.md` for the implementation and error handling.
