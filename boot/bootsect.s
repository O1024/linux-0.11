    .code16
# rewrite with AT&T syntax by falcon <wuzhangjin@gmail.com> at 081012

#
#    bootsect.s        (C) 1991 Linus Torvalds
#
# bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
# iself out of the way to address 0x90000, and jumps there.
#
# It then loads 'setup' directly after itself (0x90200), and the system
# at 0x10000, using BIOS interrupts. 
#
# NOTE! currently system is at most 8*65536 bytes long. This should be no
# problem, even in the future. I want to keep it simple. This 512 kB
# kernel size should be enough, especially as this doesn't contain the
# buffer cache as in minix
#
# The loader has been made as simple as possible, and continuos
# read errors will result in a unbreakable loop. Reboot by hand. It
# loads pretty fast by getting whole sectors at a time whenever possible.

.global _start, begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

.equ BOOTSEG, 0x07c0                    # bootsect 加载地址（上电后，BIOS 搬运第一个扇区（bootsect）到 0x7c00）
.equ INITSEG, 0x9000                    # bootsect 重定向地址
.equ SETUPSEG, 0x9020                   # setup 加载地址

# 将 bootsect（512 字节）从 0x07C00 搬运到 0x90000
    ljmp $BOOTSEG, $_start              # 段间跳转，cs = 0x07c0，执行_start
_start:
    mov $BOOTSEG, %ax
    mov %ax, %ds
    mov $INITSEG, %ax
    mov %ax, %es
    mov $256, %cx                       # 移动计数值 256 字
    sub %si, %si                        # 源地址 ds:si = 0x07C0:0x0000
    sub %di, %di                        # 目标地址 es:di = 0x9000:0x0000
    rep                                 # 重复执行 movsw 并递减寄存器 cx 的值，直到 cx = 0
    movsw                               # 从内存[ds:si]处移动cx个字到[es:di]处（1 字 = 2 字节）

    ljmp $INITSEG, $go                  # 段间跳转，cs = 0x9000，执行go
go:    
    mov %cs, %ax                        # 将ds，es，ss都设置成移动后代码所在的段处(0x9000)
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0xFF00, %sp                    # 设置栈地址为 ss:sp = 0x9000:0xff00

# 读取 setup 扇区（从软盘第 2 分区开始）到 0x90200
load_setup:
    mov $0x0000, %dx                    # [7:0] dl 表示驱动器号（0 表示第一个软盘）, [15:8] dh 表示磁头号（0 表示第一个磁头）
    mov $0x0002, %cx                    # [5:0] 表示起始扇区号（从 1 开始计数，这里是第 2 扇区），[15:6] 表示磁道号（0 表示第一个磁道）
    mov $0x0200, %bx                    # 数据要加载到的内存地址，es:bx = 0x9000:0x0200，即 0x90200
    mov $0x0204, %ax                    # [15:8] ah 为 0x02，表示调用 BIOS 中断 0x13 的读磁盘扇区服务，[7:0] al 为 0x04，表示要读取的扇区数量
    int $0x13                           # 调用 BIOS 中断 0x13，执行读磁盘操作
    jnc ok_load_setup                   # 检查进位标志 CF，如果 CF = 0（即操作成功，没有进位），则跳转到标签 ok_load_setup 处继续执行

    mov $0x0000, %dx
    mov $0x0000, %ax                
    int $0x13                           # 复位后再次尝试
    jmp load_setup

ok_load_setup:

# 获取磁盘信息并存储到 sectors
    mov $0x00, %dl                      # [0:7] dl 表示驱动器号（0 表示第一个软盘）
    mov $0x0800, %ax                    # [15:8] ah 为 0x08，表示调用 BIOS 中断 0x13 的获取磁盘参数服务
    int $0x13                           # 调用 BIOS 中断 0x13，执行获取磁盘参数的操作
    mov $0x00, %ch                      # 获取每磁道的扇区数并存储到 sectors 标签处的内存位置
    mov %cx, %cs:sectors+0

# 打印启动信息
    mov $0x03, %ah                      # 设置 BIOS 中断 0x10 的功能号为 0x03，用于读取光标位置
    xor %bh, %bh                        # 将 bh 寄存器清零，表示选择第 0 页
    int $0x10                           # 调用 BIOS 中断 0x10，读取光标位置

    mov $30, %cx                        # 打印字符数目
    mov $0x0007, %bx                    # page 0, attribute 7 (normal)
    mov $INITSEG, %ax                   # 指定 string 位置 es:bp = 0x9000:msg1
    mov %ax, %es
    mov $msg1, %bp
    mov $0x1301, %ax                    # 打印字符串并移动光标
    int $0x10

.equ SYSSEG, 0x1000                     # linux内核镜像加载地址
.equ SYSSIZE, 0x3000                    # linux内核镜像大小
.equ ENDSEG, SYSSEG + SYSSIZE           # linux内核镜像结束地址

# 加载 Linux 内核镜像
    mov $SYSSEG, %ax
    mov %ax, %es
    call load_system
    call kill_motor

# 跳转执行 setup
    ljmp $SETUPSEG, $0


#################### 以下代码实现加载 Linux 内核镜像功能 ####################
## 读取顺序：扇区 -> 磁头 -> 磁道
sread:                                  # 当前已经读取的扇区数目（含 bootsect 和 setup）
    .word 5
head:                                   # 当前磁头
    .word 0
track:                                  # 当前磁道
    .word 0

load_system:
    xor %bx, %bx                        # 清 0 段内偏移

    mov %es, %ax                        # linux内核镜像加载地址必须 64K 对齐（es = 0x1000），否则死循环执行 die
    test $0x0fff, %ax
die:    
    jne die

loop_load_kernel:
    mov %es, %ax                        # 寄存器 es 存储当前搬运的目的地址，会逐渐增加
    cmp $ENDSEG, %ax
    jb calc_next_load_sector            # 获取下一次搬运的扇区数
    ret                                 # linux内核镜像加载结束，返回 call load_system 位置

calc_next_load_sector:
    mov %cs:sectors+0, %ax
    sub sread, %ax                      # 当前磁片的当前磁道剩余待读取的扇区数

    mov %ax, %cx
    shl $9, %cx                         # 一个扇区是 512 字节（2^9），获取剩余未读字节数
    add %bx, %cx                        # cx = ax * 512 + bx，bx 是段内偏移，加上待读字节数，检查是否越界（64k）
    jc segment_overrun

# 段内加载
load_sectors_in_segment:
    call read_track

    mov %ax, %cx                        # 寄存器 cx 存储本次读取扇区数目
    add sread, %ax                      # 检查是否读完当前磁道的所有扇区，如未读完则跳转到标签 check_and_continue_load 处继续读取
    cmp %cs:sectors+0, %ax
    jne check_and_continue_load

    mov $1, %ax                         # 已读完当前磁道的所有扇区，判断是否需要更新磁头，如需要则跳转到标签 update_head_if_needed 处执行（2磁头，单磁盘？？？）
    sub head, %ax
    jne update_head_if_needed

    incw track                          # 如果当前磁头是最后一个磁头，将 track 标签处存储的当前磁道号加 1，表示要切换到下一个磁道

update_head_if_needed:
    mov %ax, head                       # 更新磁头，并清 0 已读取扇区数目
    xor %ax, %ax

check_and_continue_load:
    mov %ax, sread                      # 更新已读取的扇区数
    shl $9, %cx                         # bx = cx * 512 + bx，bx 是段内偏移，加上待读字节数，检查是否越界（64k）
    add %cx, %bx
    jnc loop_load_kernel                # 检查进位标志 CF，如果没有进位（即相加结果没有溢出），则跳转到标签 loop_load_kernel 处继续执行加载操作
    
    mov %es, %ax                        # 处理越界情况，更新寄存器 es，指向下一个 64KB 段（0x1000 -> 0x2000 -> 0x3000 -> 0x4000）
    add $0x1000, %ax
    mov %ax, %es
    xor %bx, %bx                        # 将寄存器 bx 的值清零，将内存偏移地址重置为 0
    jmp loop_load_kernel                # 跳转到标签 loop_load_kernel 进行下一段的内容加载

read_track:
    push %ax                            # 将寄存器 ax bx cx dx 的值压入栈中保存，防止后续操作修改该值影响其他部分的代码
    push %bx
    push %cx
    push %dx

    mov track, %dx                      # 将磁道号加载到寄存器 dx
    mov sread, %cx                      # 将磁道已读取的扇区数加载到寄存器 cx，+1表示待读取的起始扇区
    inc %cx
    mov %dl, %ch                        # 柱面号

    mov head, %dh                       # 设置磁盘操作的磁头号
    mov $0, %dl                         # 选择第一个磁盘驱动器
    mov $2, %ah                         # 将寄存器 ah 的值设置为 2，表示调用 BIOS 中断 0x13 的读磁盘扇区服务
    int $0x13                           # 调用 BIOS 中断 0x13，执行读磁盘操作
    
    jc bad_rt                           # 检查进位标志 CF，如果 CF = 1（即操作失败，产生进位），则跳转到标签 bad_rt 处进行错误处理
    pop %dx                             # 从栈中弹出内容，恢复寄存器 dx cx bx ax 中原来的值
    pop %cx
    pop %bx
    pop %ax 
    ret                                 # 从子程序返回，返回到调用 read_track 函数的地方

bad_rt:    
    mov    $0, %ax
    mov    $0, %dx
    int    $0x13                        # 复位

    pop    %dx                          # 从栈中弹出内容，恢复寄存器 dx cx bx ax 中原来的值
    pop    %cx
    pop    %bx
    pop    %ax
    jmp    read_track                   # 再次尝试读取

# 关闭软盘驱动器的电机，这样我们就能以已知状态进入内核，之后就无需再担心电机状态
kill_motor:
    push %dx                            # 将寄存器 dx 的值压入栈中保存
    mov $0x3f2, %dx                     # 将立即数 0x3f2 赋值给寄存器 dx。0x3f2 通常是软盘控制器的一个端口地址，该端口用于控制软盘驱动器的电机
    mov $0, %al                         # 将立即数 0 赋值给寄存器 al。这里的 0 表示要写入到 0x3f2 端口的数据，用于关闭软盘驱动器的电机
    outsb                               # 将寄存器 al 中的字节数据输出到 dx 寄存器指定的端口（即 0x3f2 端口），以此来关闭软盘驱动器的电机
    pop %dx                             # 从栈中弹出之前保存的值到寄存器 dx，恢复 dx 寄存器原来的值
    ret                                 # 从子程序返回，返回到调用该子程序的地方，结束当前子程序的执行

segment_overrun:
    xor %ax, %ax                        # 处理超过边界的情况，只读取当前段剩余空间的内容
    sub %bx, %ax
    shr $9, %ax
    jmp load_sectors_in_segment

sectors:
    .word 0

msg1:
    .byte 13,10                         # 13 表示回车符，10 表示换行符
    .ascii "IceCityOS is booting ..."
    .byte 13,10,13,10

    .org 508

# 设置根设备号 root_dev
.equ ROOT_DEV, 0x301
root_dev:
    .word ROOT_DEV

boot_flag:
    .word 0xAA55
    
    .text
    endtext:
    .data
    enddata:
    .bss
    endbss:
