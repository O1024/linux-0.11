/*
 *  linux/boot/head.s
 *
 *  Copyright (C) 1991, Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 *  # 注意！！！启动从绝对地址 0x00000000 开始，该地址也是页目录所在的位置。启动代码将被页目录覆盖
 */
.text
.globl idt, gdt, pg_dir, tmp_floppy_area, startup_32

pg_dir:

startup_32:
    movl $0x10, %eax                    # 将 ds es fs gs 均赋值为 0x10
    mov %ax, %ds                      
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # 【Linux0.11源码：08 - 重新设置 idt 和 gdt】https://www.bilibili.com/video/BV1EN411m7i5?vd_source=42b160e534344d104f989af204cd1525
    lss stack_start, %esp               # 从 stack_start 处加载栈段选择子和栈指针到 ss 和 sp 寄存器（指向 user_stack 尾部，1KB空间）
    call setup_idt                      # 设置中断描述符表（IDT）
    call setup_gdt                      # 设置全局描述符表（GDT）

    movl $0x10, %eax                    # 将 ds es fs gs 均赋值为 0x10，00010 0 00，gdt 数据段 0级别权限
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    lss stack_start, %esp               # 重新加载栈段选择子和栈指针到 ss 和 esp 寄存器
    xorl %eax, %eax                     # 将 eax 寄存器清零

# 检查 A20 地址线是否启用
1: 
    incl %eax                           # 写任意值到地址 0x000000，如果未启用 A20，访问 0x100000 实际还是访问的 0x000000
    movl %eax,0x000000
    cmpl %eax,0x100000
    je 1b

/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 *  # 注意！486 处理器应该设置第 16 位，以在超级用户模式下检查写保护。
 *  # 这样就不需要调用 "verify_area()" 函数了。
 *  # 486 用户可能还想设置 NE (#5) 位，以便使用中断 16 处理数学错误
 */
    movl %cr0, %eax                     # 将控制寄存器 cr0 的值读取到 eax 寄存器，用于检查数学协处理器
    andl $0x80000011, %eax              # 保存 PG（分页）、PE（保护模式）、ET（协处理器类型）标志位
    orl $2, %eax                        # 设置 MP（数学协处理器存在）标志位
    movl %eax, %cr0                     # 将修改后的 eax 寄存器的值写回控制寄存器 cr0
    call check_x87                      # 调用 check_x87 函数，检查数学协处理器
    jmp after_page_tables

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
    fninit                              # 初始化数学协处理器
    fstsw %ax                           # 将数学协处理器的状态字存储到 ax 寄存器
    cmpb $0, %al                        # 比较 al 寄存器的值是否为 0
    je 1f                               # 如果相等，跳转到标签 1（向前跳转），说明没有数学协处理器
    movl %cr0, %eax                     # 将控制寄存器 cr0 的值读取到 eax 寄存器
    xorl $6, %eax                       # 重置 MP 标志位，设置 EM（模拟协处理器）标志位
    movl %eax, %cr0                     # 将修改后的 eax 寄存器的值写回控制寄存器 cr0
    ret

.align 2
1:
    .byte 0xDB,0xE4                     # 287 数学协处理器的 fsetpm 指令，387 协处理器会忽略该指令
    ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 *  # 设置一个包含 256 个条目的 IDT，每个条目都指向 ignore_int 中断处理函数。
 *  # 然后加载 IDT。任何想要在 IDT 表中安装自己的中断处理函数的模块都可以自行操作。
 *  # 当中断处理函数准备好后，会在其他地方启用中断。该函数的代码将被页目录覆盖
 */
setup_idt:
    lea ignore_int, %edx                # edx = &ignore_int = 0x5428
    movl $0x00080000, %eax              # eax = 0x00080000, 0001 0 00，gdt 代码段 0级别权限
    movw %dx, %ax                       # eax = eax | edx = 0x00080000 | 0x5428 = 0x00085428
    movw $0x8E00, %dx                   # dx = 0x8E00，0x8E00 表示中断门，DPL=0，存在标志位为 1
    lea idt, %edi                       # edi = &idt = 0x54b8
    mov $256, %ecx                      # 将立即数 256 移动到 ecx 寄存器，作为循环计数器

rp_sidt:
    movl %eax, (%edi)                   # *edi = eax = 0x00085428
    movl %edx, 4(%edi)                  # *(edi + 4) = edx = 0x8e00
    addl $8, %edi                       # edi += 8
    dec %ecx                            # ecx -= 1
    jne rp_sidt                         # 如果 ecx 寄存器的值不为 0，跳转到 rp_sidt 继续加载
    lidt idt_descr                      # 加载中断描述符表寄存器（IDTR），使用 idt_descr 中的信息
    ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 *  # 该函数设置一个新的 GDT 并加载它。目前只构建了两个条目，与 init.s 中构建的相同。
 *  # 该函数只有两行代码，但注释很长，因为它很重要。该函数的代码将被页目录覆盖
 */
setup_gdt:
    lgdt gdt_descr                      # 加载全局描述符表寄存器（GDTR），使用 gdt_descr 中的信息
    ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 *  # 我将内核页表放在页目录之后，使用 4 个页表来映射 16MB 的物理内存。
 *  # 拥有超过 16MB 内存的用户需要扩展这部分代码
 */
.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 *  # tmp_floppy_area 是软盘驱动程序在 DMA 无法访问缓冲区块时使用的区域。
 *  # 它需要对齐，以避免位于 64KB 边界上
 */
tmp_floppy_area:
    .fill 1024,1,0                  # 填充 1024 个字节，每个字节的值为 0

after_page_tables:
    pushl $0                          # 将 0 0 0 压入栈中，作为 main 函数的参数
    pushl $0
    pushl $0
    pushl $L6                          # 将标签 L6 的地址压入栈中，作为 main 函数的返回地址
    pushl $main                      # 将 main 函数的地址压入栈中
    jmp setup_paging                  # 跳转到 setup_paging 函数，设置分页机制
L6:
    jmp L6                          # 无限循环，main 函数不应该返回到这里，但以防万一
                                    # 这里的无限循环确保系统不会执行不可预期的代码

/* This is the default interrupt "handler" :-) */
int_msg:
    .asciz "Unknown interrupt\n\r"  # 定义一个以 null 结尾的字符串，用于输出未知中断信息

.align 2
ignore_int:
    pushl %eax                          # 将 eax ecx edx ds es fs 寄存器的值压入栈中，保存现场
    pushl %ecx
    pushl %edx
    push %ds  
    push %es  
    push %fs

    movl $0x10, %eax                    # 设置 ds es fs = 0x10
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs

    pushl $int_msg                      # 将 int_msg 字符串的地址压入栈中，作为 printk 函数的参数
    call printk                         # 调用 printk 函数，输出未知中断信息

    popl %eax                           # 从栈中弹出值到 eax ecx edx ds es fs 寄存器，恢复现场
    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    
    iret                              # 中断返回，恢复中断前的状态

/*
 * Setup_paging
 *  # 设置分页机制的函数
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *  # 该函数通过设置控制寄存器 cr0 中的分页标志位来启用分页机制。
 *  # 设置页表，将前 16MB 的物理内存进行恒等映射。分页器假设不会产生非法地址
 *  # （例如在 4MB 内存的机器上访问超过 4MB 的地址）
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *  # 注意！尽管该函数将所有物理内存进行了恒等映射，但只有内核页函数会直接使用超过 1MB 的地址。
 *  # 所有 "正常" 函数只使用低于 1MB 的地址，或者本地数据空间，这些地址会被映射到其他位置 - 内存管理模块会跟踪这些映射
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 *  # 对于拥有超过 16MB 内存的用户来说，很遗憾。我没有这样的机器，所以代码只支持 16MB。
 *  # 源代码在这里，你可以自行修改。（说真的，修改应该不难，主要是修改一些常量等。
 *  # 我将内存限制在 16MB，因为我的机器甚至无法扩展到更大的内存（不过它很便宜 :-)
 *  # 我已经在一些常量旁边做了标记（搜索 "16Mb"），但不能保证这就是所有需要修改的地方 :-( ）
 */
# 【Linux0.11源码：09 - Intel 内存管理两板斧：分段与分页】https://www.bilibili.com/video/BV1214y1o7J4?vd_source=42b160e534344d104f989af204cd1525
.align 2
setup_paging:
    # 从地址 0 开始清 0，大小 0x1400，5 个页面
    movl $1024*5, %ecx                  # 将立即数 1024*5 移动到 ecx 寄存器，作为循环计数器，用于初始化 5 个页表（页目录 + 4 个页表）
    xorl %eax, %eax                     # 将 eax 寄存器清零
    xorl %edi, %edi                     # 将 edi 寄存器清零，edi 指向页目录的起始地址 0x000
    cld                                 # 清除方向标志位 DF，使串操作指令按地址递增方向执行
    rep                                 # 重复执行 stosl 指令，将 eax 寄存器的值存储到 edi 指向的内存地址，直到 ecx 寄存器的值为 0
    stosl

    # 设置页目录表
    movl $pg0 + 7, pg_dir               # *0x0 = 页表 0 的地址 + 7（设置存在位、用户可读写位）
    movl $pg1 + 7, pg_dir + 4
    movl $pg2 + 7, pg_dir + 8
    movl $pg3 + 7, pg_dir + 12

    # 设置页表项 pg3 pg2 pg1 pg0，包含地址和属性，*(0x4000 + 4092) = 0xfff007; *(0x4000 + 4088) = 0x0xffe007; 
    movl $pg3 + 4092, %edi              # 将页表 3 的最后一个条目的地址加载到 edi 寄存器
    movl $0xfff007, %eax                # 0xfff007 表示 16MB - 4096 + 7（用户可读写、存在位）
    std                                 # 设置方向标志位 DF，使串操作指令按地址递减方向执行
1:
    stosl                               # 将 eax 寄存器的值存储到 edi 指向的内存地址，填充页表条目
    subl $0x1000, %eax                  # 将 eax 寄存器的值减去 0x1000，指向前一个页框，1 页 4k
    jge 1b                              # 共设置 4096 个页表

    # 保存页目录表地址并启动分页
    cld                                 # 清除方向标志位 DF，使串操作指令按地址递增方向执行
    xorl %eax, %eax                     # 将 eax 寄存器清零
    movl %eax, %cr3                     # 将 0x00000000 存储到控制寄存器 cr3，设置页目录的起始地址
    movl %cr0, %eax                     # 将控制寄存器 cr0 的值读取到 eax 寄存器
    orl $0x80000000, %eax               # 设置 eax 寄存器的第 31 位，即分页标志位 PG
    movl %eax, %cr0                     # 将修改后的 eax 寄存器的值写回控制寄存器 cr0，启用分页机制
    ret                                 # 调用栈中压入地址，并返回执行（之前压入 main 地址）

.align 2
    .word 0
idt_descr:
    .word 256*8-1                      # 定义 IDT 表的界限，256 个条目，每个条目 8 字节
    .long idt                          # 定义 IDT 表的基地址

.align 2
    .word 0
gdt_descr:
    .word 256*8-1                      # 定义 GDT 表的界限，256 个条目，每个条目 8 字节
    .long gdt                          # 定义 GDT 表的基地址

.align 8                              # 按 8 字节对齐
idt:
    .fill 256,8,0                      # 填充 256 个条目，每个条目 8 字节，初始值为 0，IDT 表初始化为空

gdt:
    .quad 0x0000000000000000            # 定义 GDT 的第一个条目，空描述符
    .quad 0x00c09a0000000fff            # 定义代码段描述符，00c0 ; 9a00 (1 00 1 1 0 1 0 00000000); 0000 段起始地址; 0fff limit 对应 4096，页 4KB，共 16MB
    .quad 0x00c0920000000fff            # 定义数据段描述符，00c0 ; 9200 (1 00 1 0 0 1 0 00000000); 0000 段起始地址; 0fff limit 对应 4096，页 4KB，共 16MB
    .quad 0x0000000000000000            # 临时描述符，暂不使用
    .fill 252,8,0                       # 填充 252 个条目，每个条目 8 字节，初始值为 0，为 LDT 和 TSS 等预留空间
