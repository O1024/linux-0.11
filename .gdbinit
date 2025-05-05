set confirm off

target remote :1234
layout src
layout regs
b main
focus cmd
c