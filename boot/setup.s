.code16                                # 指定使用 16 位代码模式
# rewrite with AT&T syntax by falcon <wuzhangjin@gmail.com> at 081012
#
#    setup.s        (C) 1991 Linus Torvalds
#
# setup.s is responsible for getting the system data from the BIOS,
# and putting them into the appropriate places in system memory.
# both setup.s and system has been loaded by the bootblock.
#
# This code asks the bios for memory/disk/other parameters, and
# puts them in a "safe" place: 0x90000-0x901FF, ie where the
# boot-block used to be. It is then up to the protected mode
# system to read them from there before the area is overwritten
# for buffer-blocks.
#

.global _start, begtext, begdata, begbss, endtext, enddata, endbss  # 声明全局符号
.text
begtext:
.data
begdata:
.bss
begbss:
.text

# NOTE! These had better be the same as in bootsect.s!

.equ INITSEG, 0x9000                    # bootsect 重定向地址
.equ SYSSEG, 0x1000                     # linux 内核镜像加载地址
.equ SETUPSEG, 0x9020                   # setup 加载地址

    ljmp $SETUPSEG, $_start             # 段间跳转，cs = 0x9020，执行_start
_start:
    mov %cs, %ax                        # 将ds，es设置成移动后代码所在的段处(0x9020)
    mov %ax, %ds
    mov %ax, %es

# 打印 setup 启动信息
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置

    mov $29, %cx                        # 设置要打印的字符数量为 29
    mov $0x000b, %bx                    # 设置显示属性，页号为 0，属性为 0xb
    mov $setup_start_msg, %bp           # 将 setup_start_msg 标签的地址加载到 bp 寄存器，作为要打印的字符串地址
    mov $0x1301, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x1301，用于打印字符串并移动光标
    int $0x10                           # 调用 BIOS 中断 0x10，打印字符串

# 保存当前光标位置到 0x9000:0x0000
    mov $INITSEG, %ax                   # 设置数据段寄存器 ds 为0x9000
    mov %ax, %ds

    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置
    mov %dx, %ds:0                      # 将光标位置（dx 寄存器）保存到 0x90000 处

# 保存扩展内存大小到 0x9000:0x0002
    mov $0x88, %ah                      # 设置 BIOS 中断 0x15 的功能号为 0x88，用于获取扩展内存大小
    int $0x15                           # 调用 BIOS 中断 0x15，获取扩展内存大小
    mov %ax, %ds:2                      # 将获取到的扩展内存大小（ax 寄存器）保存到 0x90002 处

# 保存显卡信息，页号 0x9000:0x0004，视频模式 0x9000: 0x0006
    mov $0x0f, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x0f，用于获取当前视频模式信息
    int $0x10                           # 调用 BIOS 中断 0x10，获取当前视频模式信息
    mov %bx, %ds:4                      # 将显示页号（bh 寄存器的值）保存到 0x90004 处
    mov %ax, %ds:6                      # 将视频模式（al 寄存器的值）和窗口宽度（ah 寄存器的值）保存到 0x90006 处

# check for EGA/VGA and some config parameters
    mov $0x12, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x12，用于检查 EGA/VGA 及相关配置参数
    mov $0x10, %bl                      # 设置 bl 寄存器的值为 0x10，作为子功能号
    int $0x10                           # 调用 BIOS 中断 0x10，检查 EGA/VGA 及相关配置参数
    mov %ax, %ds:8                      # 将返回的信息（ax 寄存器的值）保存到 0x90008 处
    mov %bx, %ds:10                     # 将返回的信息（bx 寄存器的值）保存到 0x9000A 处
    mov %cx, %ds:12                     # 将返回的信息（cx 寄存器的值）保存到 0x9000C 处

# 保存硬盘 hd0 参数表
    mov $0x0000, %ax                    # 清 0 数据段寄存器 ds
    mov %ax, %ds
    lds %ds:4*0x41, %si                 # 从 0x0000:0x104 处加载段地址和偏移地址到 ds:si 寄存器对

    mov $INITSEG, %ax                   # es:di = 0x9000:0x0080
    mov %ax, %es
    mov $0x0080, %di
    mov $0x10, %cx                      # 从 ds:si 处复制 0x10 个字节到 es:di 处
    rep 
    movsb

# 保存硬盘 hd1 参数表
    mov $0x0000, %ax                    # 清 0 数据段寄存器 ds
    mov %ax, %ds
    lds %ds:4*0x46, %si                 # 从 0x0000:0x118 处加载段地址和偏移地址到 ds:si 寄存器对

    mov $INITSEG, %ax
    mov %ax, %es
    mov $0x0090, %di                    # es:di = 0x9000:0x0090
    mov $0x10, %cx                      # 从 ds:si 处复制 0x10 个字节到 es:di 处
    rep 
    movsb

# 设置寄存器 ds 和 es
    mov $INITSEG, %ax                   # ds = 0x9000
    mov %ax, %ds
    mov $SETUPSEG, %ax                  # es = 0x9020
    mov %ax, %es

# 打印光标位置
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置
 
    mov $12, %cx                        # 设置要打印的字符数量为 12（含空格）
    mov $0x000c, %bx                    # 设置显示属性，页号为 0，属性为 0xc
    mov $cur_pos_info, %bp              # 将 cur_pos_info 标签的地址加载到 bp 寄存器，作为要打印的字符串地址
    mov $0x1301, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x1301，用于打印字符串并移动光标
    int $0x10                           # 调用 BIOS 中断 0x10，打印字符串

    mov %ds:0, %ax                      # 打印 0x90000 处保存的光标位置
    call print_hex
    call print_nl

# 打印扩展内存大小
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置

    mov $17, %cx                        # 设置要打印的字符数量为 17（含空格）
    mov $0x000a, %bx                    # 设置显示属性，页号为 0，属性为 0xa
    mov $mem_size_info, %bp             # 将 mem_size_info 标签的地址加载到 bp 寄存器，作为要打印的字符串地址
    mov $0x1301, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x1301，用于打印字符串并移动光标
    int $0x10                           # 调用 BIOS 中断 0x10，打印字符串

    mov %ds:2 , %ax                     # 打印 0x90000 处保存的扩展内存大小
    call print_hex
    call print_nl

# 打印硬盘信息
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置

    mov $22, %cx                        # 设置要打印的字符数量为 22
    mov $0x000d, %bx                    # 设置显示属性，页号为 0，属性为 0xd
    mov $hd_info, %bp                   # 将 hd_info 标签的地址加载到 bp 寄存器，作为要打印的字符串地址
    mov $0x1301, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x1301，用于打印字符串并移动光标
    int $0x10                           # 调用 BIOS 中断 0x10，打印字符串

    mov %ds:0x80, %ax                   # 打印 0x90080 处保存的硬盘信息
    call print_hex
    call print_nl

# 打印硬盘磁头信息
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置

    mov $11, %cx                        # 设置要打印的字符数量为 8
    mov $0x000e, %bx                    # 设置显示属性，页号为 0，属性为 0xe
    mov $hd_header_info, %bp            # 将 hd_header_info 标签的地址加载到 bp 寄存器，作为要打印的字符串地址
    mov $0x1301, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x1301，用于打印字符串并移动光标
    int $0x10                           # 调用 BIOS 中断 0x10，打印字符串

    mov %ds:0x82, %ax                   # 打印 0x90082 处保存的硬盘信息
    call print_hex
    call print_nl

# 打印磁盘扇区信息
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置

    mov $11, %cx                        # 设置要打印的字符数量为 8
    mov $0x000f, %bx                    # 设置显示属性，页号为 0，属性为 0xf
    mov $hd_sect_info, %bp              # 将 hd_sect_info 标签的地址加载到 bp 寄存器，作为要打印的字符串地址
    mov $0x1301, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x1301，用于打印字符串并移动光标
    int $0x10                           # 调用 BIOS 中断 0x10，打印字符串

    mov %ds:0x8e, %ax                   # 打印 0x9008E 处保存的硬盘扇区
    call print_hex
    call print_nl
    call print_nl

# 保存最终光标位置到 0x9000:0x0000，防止内核打印覆盖之前的打印
    mov $INITSEG, %ax                   # 设置数据段寄存器 ds 为0x9000
    mov %ax, %ds

    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置
    mov %dx, %ds:0                      # 将光标位置（dx 寄存器）保存到 0x90000 处

# 检查硬盘 hd1 是否存在
    mov $0x01500, %ax                   # 将 0x01500 加载到 ax 寄存器，作为 BIOS 中断 0x13 的功能号
    mov $0x81, %dl                      # 将 0x81 加载到 dl 寄存器，表示选择第二个硬盘
    int $0x13                           # 调用 BIOS 中断 0x13，检查第二个硬盘是否存在
    jc no_disk1                         # 如果进位标志 CF 为 1，说明操作失败，跳转到 no_disk1 标签处
    cmp $3, %ah                         # 比较 ah 寄存器的值是否为 3
    je is_disk1                         # 如果相等，说明第二个硬盘存在，跳转到 is_disk1 标签处

# 不存在硬盘 hd1 则清 0 0x90090 0x10 字节
no_disk1:
    mov $INITSEG, %ax                   # 将 INITSEG 的值加载到 ax 寄存器
    mov %ax, %es                        # 将 ax 的值复制到附加段寄存器 es
    mov $0x0090, %di                    # 将 0x0090 加载到 di 寄存器，作为目标地址偏移
    mov $0x10, %cx                      # 设置要复制的字节数为 0x10
    mov $0x00, %ax                      # 将 ax 寄存器清零
    rep                                 # 重复执行 stosb 指令，直到 cx 寄存器的值为 0
    stosb                               # 将 al 寄存器的值存储到 es:di 处

is_disk1:

# 以下代码将实现保护模式切换
    cli                                 # 禁止中断，清除中断标志位 IF

# 将 Linux 内核镜像从 0x10000 - 0x90000 搬运到 0x00000 - 0x80000，整体前移 0x10000
    mov $0x0000, %ax                    # 将 ax 寄存器清零
    cld                                 # 清除方向标志位 DF，使串操作指令按地址递增方向执行
relocate_kernel_image:
    mov %ax, %es                        # es = 0x0000 -> 0x1000 -> ...
    add $0x1000, %ax
    cmp $0x9000, %ax
    jz end_move                         # 如果相等，说明移动完成，跳转到 end_move 标签处
    mov %ax, %ds                        # ds = 0x1000 -> 0x2000 -> ...
    sub %di, %di                        # 将 di 寄存器清零，作为目标地址偏移
    sub %si, %si                        # 将 si 寄存器清零，作为源地址偏移
    mov $0x8000, %cx                    # 每次搬运 64KB
    rep                                 # 重复执行 movsw 指令，直到 cx 寄存器的值为 0
    movsw                               # 从 ds:si 处复制一个字到 es:di 处
    jmp relocate_kernel_image           # 跳转到 relocate_kernel_image 标签处，继续移动

# 搬运 Linux 内核镜像后加载段描述符 idt 和 gdt
end_move:
    mov $SETUPSEG, %ax                  # ds = 0x9020
    mov %ax, %ds
    lidt idt_48                         # 加载中断描述符表寄存器（IDTR）
    lgdt gdt_48                         # 加载全局描述符表寄存器（GDTR）

    inb $0x92, %al                      # 从 0x92 端口读取数据到 al 寄存器
    orb $0b00000010, %al                # 将 al 寄存器的值与 0b00000010 按位或，打开 A20 线
    outb %al, $0x92                     # 将 al 寄存器的值输出到 0x92 端口

# well, that went ok, I hope. Now we have to reprogram the interrupts :-(
# we put them right after the intel-reserved hardware interrupts, at
# int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
# messed this up with the original PC, and they haven't been able to
# rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
# which is used for the internal hardware interrupts as well. We just
# have to reprogram the 8259's, and it isn't fun.

    mov $0x11, %al                      # 初始化 8259A 芯片，发送初始化命令字 ICW1
                                        # 需要 ICW4，级联模式，电平触发
    out %al, $0x20                      # 发送到主 8259A 芯片的命令端口 0x20
    .word 0x00eb,0x00eb                 # 短跳转，延时等待芯片响应
    out %al, $0xA0                      # 发送到从 8259A 芯片的命令端口 0xA0
    .word 0x00eb,0x00eb
    mov $0x20, %al                      # 设置主 8259A 芯片的中断向量基地址为 0x20，发送 ICW2
    out %al, $0x21                      # 发送到主 8259A 芯片的数据端口 0x21
    .word 0x00eb,0x00eb
    mov $0x28, %al                      # 设置从 8259A 芯片的中断向量基地址为 0x28，发送 ICW2
    out %al, $0xA1                      # 发送到从 8259A 芯片的数据端口 0xA1
    .word 0x00eb,0x00eb                 # IR 7654 3210
    mov $0x04, %al                      # 设置主 8259A 芯片为级联主片，发送 ICW3
    out %al, $0x21                      # 发送到主 8259A 芯片的数据端口 0x21
    .word 0x00eb,0x00eb
    mov $0x02, %al                      # 设置从 8259A 芯片为级联从片，连接到主片的 IR2 引脚，发送 ICW3
    out %al, $0xA1
    .word 0x00eb,0x00eb
    mov $0x01, %al                      # 设置 8259A 芯片为 8086 模式，发送 ICW4
    out %al, $0x21
    .word 0x00eb,0x00eb
    out %al, $0xA1
    .word 0x00eb,0x00eb
    mov $0xFF, %al                      # 屏蔽所有中断，设置中断屏蔽寄存器（IMR）
    out %al, $0x21
    .word 0x00eb,0x00eb
    out %al, $0xA1

# well, that certainly wasn't fun :-(. Hopefully it works, and we don't
# need no steenking BIOS anyway (except for the initial loading :-).
# The BIOS-routine wants lots of unnecessary data, and it's less
# "interesting" anyway. This is how REAL programmers do it.
#
# Well, now's the time to actually move into protected mode. To make
# things as simple as possible, we do no register set-up or anything,
# we let the gnu-compiled 32-bit programs do that. We just jump to
# absolute address 0x00000, in 32-bit protected mode.

# 【Linux0.11源码：07 - 六行代码进入保护模式】https://www.bilibili.com/video/BV1YF411R7dD?vd_source=42b160e534344d104f989af204cd1525
    mov %cr0, %eax                      # 读取控制寄存器 CR0 的值到 eax 寄存器
    bts $0, %eax                        # 将 eax 寄存器的第 0 位（保护模式位 PE）置 1
    mov %eax, %cr0                      # 将修改后的 eax 寄存器的值写回控制寄存器 CR0，启用保护模式

# 【Linux0.11源码：08 - 重新设置 idt 和 gdt】https://www.bilibili.com/video/BV1EN411m7i5?vd_source=42b160e534344d104f989af204cd1525
.equ sel_cs0, 0x0008                    # 段选择子，INDEX:TI:RPL = 001:0:00
    ljmp $sel_cs0, $0                   # 跳转到全局描述符表中代码段 0 的偏移地址 0 处（此时处于保护模式）

gdt:
    .word 0,0,0,0                       # 空描述符，作为全局描述符表的第一个条目

    .word 0x07FF                        # 段界限为 2047 (2048*4096=8Mb)
    .word 0x0000                        # 段基地址为 0
    .word 0x9A00                        # 代码段描述符，可读可执行
    .word 0x00C0                        # 粒度为 4096 字节，386 模式

    .word 0x07FF                        # 段界限为 2047 (2048*4096=8Mb)
    .word 0x0000                        # 段基地址为 0
    .word 0x9200                        # 数据段描述符，可读可写
    .word 0x00C0                        # 粒度为 4096 字节，386 模式

idt_48:
    .word 0                             # 中断描述符表的界限为 0
    .word 0,0                           # 中断描述符表的基地址为 0L

# 【Linux0.11源码：06 - 解决段寄存器的历史包袱问题】https://www.bilibili.com/video/BV1zk4y1N7pk?vd_source=42b160e534344d104f989af204cd1525
gdt_48:
    .word 0x800                         # 全局描述符表的界限为 2048，可容纳 256 个描述符（8个字节表示1组描述符）
    .word 0x200 + gdt, 0x9              # 指定全局描述符表的 gdt 基地址为 0X9xxxx，0x200 是因为 setup 被搬运到 0x90200 位置

# 以 16进制形式打印
print_hex:
    push %ax
    mov $0x0e30, %ax                    # 打印 0x 字符串
    int $0x10
    mov $0x0e78, %ax
    int $0x10
    pop %ax

    mov $4, %cx                         # 设置循环次数为 4，因为一个 16 位整数需要 4 个十六进制字符表示
    mov %ax, %dx                        # 将 ax 寄存器的值复制到 dx 寄存器

print_digit:    
    rol $4, %dx                         # 将 dx 寄存器的值循环左移 4 位，使低 4 位成为高 4 位
    mov $0xe0f, %ax                     # 设置 BIOS 中断 0x10 的功能号为 0x0e，al 寄存器的值为 0x0f，用于显示字符
    and %dl, %al                        # 将 dl 寄存器的低 4 位与 al 寄存器的值按位与
    add $0x30, %al                      # 将结果转换为对应的 ASCII 码
    cmp $0x3a, %al                      # 比较结果是否大于等于 0x3a（即 'A' 的 ASCII 码）
    jl outp                             # 如果小于 0x3a，直接输出
    add $0x07, %al                      # 如果大于等于 0x3a，加上 0x07，转换为大写字母的 ASCII 码

outp:
    int $0x10                           # 调用 BIOS 中断 0x10，输出字符
    loop print_digit                    # 循环 4 次，直到输出完所有十六进制字符
    ret

# 打印回车符和换行符
print_nl:
    mov $0x0e0d, %ax                    # 设置 BIOS 中断 0x10 的功能号为 0x0e，al 寄存器的值为 0x0d，用于输出回车符
    int $0x10                           # 调用 BIOS 中断 0x10，输出回车符
    mov $0x0e0a, %ax                    # 将 al 寄存器的值设置为 0x0a，用于输出换行符
    int $0x10                           # 调用 BIOS 中断 0x10，输出换行符
    ret

setup_start_msg:
    .byte 13,10
    .ascii "Now we are in setup ..."
    .byte 13,10,13,10

cur_pos_info:
    .ascii "Cursor POS: "

mem_size_info:
    .ascii "Memory SIZE(KB): "
    .byte 13,10,13,10

hd_info:
    .ascii "HD Info"
    .byte 13,10
    .ascii "  Cylinders: "

hd_header_info:
    .ascii "  Headers: "

hd_sect_info:
    .ascii "  Sectors: "

.text
endtext:
.data
enddata:
.bss
endbss:
