; A simple program to demonstrate the stack without breakpoints.
; We push 3 recognizable hex values to the stack and HALT immediately.

LDI R1, 0x1111
LDI R2, 0x2222
LDI R3, 0x3333

PUSH R1
PUSH R2
PUSH R3

HALT
