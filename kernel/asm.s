/*
 *  linux/kernel/asm.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * asm.s contains the low-level code for most hardware faults.
 * page_exception is handled by the mm, so that isn't here. This
 * file also handles (hopefully) fpu-exceptions due to TS-bit, as
 * the fpu must be properly saved/resored. This hasn't been tested.
 */

.globl divide_error,debug,nmi,int3,overflow,bounds,invalid_op
.globl double_fault,coprocessor_segment_overrun
.globl invalid_TSS,segment_not_present,stack_segment
.globl general_protection,coprocessor_error,irq13,reserved

divide_error:
    pushl $do_divide_error  			# *esp = &do_divide_error
no_error_code:
    xchgl %eax,(%esp)       			# eax = &do_divide_error, *esp = eax
    pushl %ebx              			# 将 ebx ecx edx edi esi ebp 寄存器的值压入栈中，保存现场
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    push %ds                			# 将数据段寄存器 ds 的值压入栈中，保存现场
    push %es                			# 将附加段寄存器 es 的值压入栈中，保存现场
    push %fs                			# 将附加段寄存器 fs 的值压入栈中，保存现场
    pushl $0                			# 压入一个 0 作为“错误码”，因为除零错误没有实际的错误码

    lea 44(%esp),%edx       			# *edx = esp + 44，此地址指向保存的寄存器值
    pushl %edx              			# 将 esp0 压入栈中（esp 指向存储 eax）
    movl $0x10,%edx         			# edx = 0x10
    mov %dx,%ds             			# 将 edx 寄存器的低 16 位赋值给数据段寄存器 ds
    mov %dx,%es             			# 将 edx 寄存器的低 16 位赋值给附加段寄存器 es
    mov %dx,%fs             			# 将 edx 寄存器的低 16 位赋值给附加段寄存器 fs
    call *%eax              			# 执行 eax 指向的函数（通常在进入中断后将该函数地址压入栈）

    addl $8,%esp            			# 调整栈指针，跳过之前压入的错误码和地址

    pop %fs                 			# 从栈中弹出值到附加段寄存器 fs，恢复现场
    pop %es                 			# 从栈中弹出值到附加段寄存器 es，恢复现场
    pop %ds                 			# 从栈中弹出值到数据段寄存器 ds，恢复现场
    popl %ebp               			# 从栈中弹出值到 ebp esi edi edx ecx ebx eax 寄存器，恢复现场
    popl %esi
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret                    			# 从中断返回，恢复程序执行

debug:
    pushl $do_int3        				# _do_debug
    jmp no_error_code

nmi:
    pushl $do_nmi
    jmp no_error_code

int3:
    pushl $do_int3
    jmp no_error_code

overflow:
    pushl $do_overflow
    jmp no_error_code

bounds:
    pushl $do_bounds
    jmp no_error_code

invalid_op:
    pushl $do_invalid_op
    jmp no_error_code

coprocessor_segment_overrun:
    pushl $do_coprocessor_segment_overrun
    jmp no_error_code

reserved:
    pushl $do_reserved
    jmp no_error_code

irq13:
    pushl %eax
    xorb %al,%al
    outb %al,$0xF0
    movb $0x20,%al
    outb %al,$0x20
    jmp 1f
1:    jmp 1f
1:    outb %al,$0xA0
    popl %eax
    jmp coprocessor_error

double_fault:
    pushl $do_double_fault
error_code:
    xchgl %eax,4(%esp)        # error code <-> %eax
    xchgl %ebx,(%esp)        # &function <-> %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    push %ds
    push %es
    push %fs
    pushl %eax            # error code
    lea 44(%esp),%eax        # offset
    pushl %eax
    movl $0x10,%eax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    call *%ebx
    addl $8,%esp
    pop %fs
    pop %es
    pop %ds
    popl %ebp
    popl %esi
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret

invalid_TSS:
    pushl $do_invalid_TSS
    jmp error_code

segment_not_present:
    pushl $do_segment_not_present
    jmp error_code

stack_segment:
    pushl $do_stack_segment
    jmp error_code

general_protection:
    pushl $do_general_protection
    jmp error_code

