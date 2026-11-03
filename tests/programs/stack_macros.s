; Full-descending stack operations built from preprocessor macros.
; SAVE pushes a pair in argument order, RESTORE pops it back in reverse order,
; so the two calls leave R1, R2 and R3 unchanged.

.macro SAVE a, b
  PUSH \a
  PUSH \b
.endmacro

.macro RESTORE a, b
  POP \b
  POP \a
.endmacro

LDI R1, 0x1111
LDI R2, 0x2222
LDI R3, 0x3333

SAVE R1, R2
SAVE R3, R1

RESTORE R3, R1
RESTORE R1, R2

HALT
