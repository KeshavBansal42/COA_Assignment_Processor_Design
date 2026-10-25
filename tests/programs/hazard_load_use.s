; A store immediately followed by a load of the same address, then the loaded
; value is used straight away.
LDI R1, 100
LDI R2, 0x800
SW R1, 0(R2)
LW R3, 0(R2)
ADD R4, R3, R1
HALT
