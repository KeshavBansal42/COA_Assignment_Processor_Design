# RISC201 Assembly EBNF Grammar

```ebnf
program = { statement } ;
statement = [ label ] , [ instruction ] , [ comment ] , newline ;

label = identifier , ":" ;
instruction = opcode , [ operands ] ;

operands = r_operands | i_operands | j_operands | push_pop_operands ;
r_operands = register , "," , register , "," , register ;
i_operands = register , "," , register , "," , immediate ;
j_operands = offset ;
push_pop_operands = register ;

opcode = "ADD" | "SUB" | "MUL" | "DIV" | "AND" | "OR" | "XOR" | "NOT" |
         "SHL" | "SHR" | "MOV" | "CMP" | "ADDI" | "LDI" | "LW" | "SW" |
         "PUSH" | "POP" | "RET" | "JMP" | "BEQ" | "BNE" | "BLT" | "BGE" |
         "CALL" | "HALT" | "NOP" ;

register = "r0" | "r1" | "r2" | "r3" | "r4" | "r5" | "r6" | "r7" |
           "r8" | "r9" | "r10" | "r11" | "r12" | "r13" | "r14" | "r15" | 
           "sp" | "pc" | "flags" ;

immediate = [ "-" ] , digit , { digit } ;
offset = identifier | immediate ;
identifier = letter , { letter | digit | "_" } ;
comment = ";" , { any_character_except_newline } ;

letter = "A" | "B" | ... | "Z" | "a" | "b" | ... | "z" ;
digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
newline = "\n" ;
```
