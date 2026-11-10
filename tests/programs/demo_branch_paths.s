; BNE is not taken; BEQ is taken and must discard the wrong-path write.
LDI R1, 5
LDI R2, 5
CMP R1, R2
BNE wrong
LDI R3, 7
CMP R1, R2
BEQ taken
LDI R4, 99
taken:
LDI R4, 42
HALT
wrong:
LDI R3, 99
HALT
