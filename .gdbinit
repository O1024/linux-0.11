set confirm off

target remote :1234
layout src
layout regs
focus cmd

define debug_bootsect
    file tools/bootsect

    # _start
    b *0x7c00
    # ok_load_setup
    b *0x90042
    # load_system
    b *0x9007f
    # kill_motor
    b *0x90106

    # end_move
    b *0x90364
    c
end

define debug_main
    file tools/system
    b main
    c
end

# load && c 重新开始调试

