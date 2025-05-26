set confirm off

define display_stack
    i registers ss
    i registers sp
    x/32x &user_stack[0x400-0x20]
end

define display_current
    watch current
    watch jiffies

    display current
    display current->pid
    display current->counter
    display jiffies
end

define debug_bootsect
    target remote :1234
    layout src
    layout regs
    focus cmd

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
    target remote :1234
    layout split
    focus cmd

    file tools/system
    b main.c:main
    b main.c:init
    b sched.c:schedule
    c
end

# load && c 重新开始调试

