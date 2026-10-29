# Build binary32 constants 1.5 and 2.0 from their upper 16 bits.
LDI R1, 16320
LDI R2, 16
SHL R1, R1, R2
LDI R3, 16384
SHL R3, R3, R2
FADD R4, R1, R3
FMUL R5, R4, R3
FDIV R6, R5, R3
HALT
