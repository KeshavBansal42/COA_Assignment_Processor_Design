; A compare immediately followed by a branch that depends on its flags.
LDI R1, 10
LDI R2, 10
CMP R1, R2
BEQ done
LDI R3, 111
done:
LDI R3, 222
HALT
