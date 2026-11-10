; Older register and memory writes complete; younger writes after HALT do not.
LDI R1, 2048
LDI R2, 42
SW R2, 0(R1)
LDI R3, 17
HALT
LDI R3, 99
LDI R2, 99
SW R2, 0(R1)
