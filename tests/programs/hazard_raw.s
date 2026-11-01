; Back to back read after write dependencies, no load or branch involved.
LDI R1, 5
LDI R2, 7
ADD R3, R1, R2
ADD R4, R3, R1
ADD R5, R4, R3
MUL R6, R5, R4
HALT
