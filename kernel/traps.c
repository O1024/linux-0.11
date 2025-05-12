/*
 *  linux/kernel/traps.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'. Currently mostly a debugging-aid, will be extended
 * to mainly kill the offending process (probably by giving it a signal,
 * but possibly by killing it outright if necessary).
 */
#include <string.h> 

#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#define get_seg_byte(seg,addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
    :"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
    :"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define _fs() ({ \
register unsigned short __res; \
__asm__("mov %%fs,%%ax":"=a" (__res):); \
__res;})

int do_exit(long code);

void page_exception(void);

void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void reserved(void);
void parallel_interrupt(void);
void irq13(void);

/**
 * @brief 处理致命错误并打印相关信息，最后退出当前进程
 * 
 * 当系统遇到严重错误时，此函数会打印错误信息、寄存器状态、堆栈信息等，
 * 并终止当前进程。
 * 
 * @param str 错误信息字符串
 * @param esp_ptr 指向堆栈指针的长整型值
 * @param nr 错误编号
 */
static void die(char *str, long esp_ptr, long nr)
{
    long *esp = (long *)esp_ptr;
    int i;

    // 打印错误信息和错误编号
    printk("%s: %04x\n\r", str, nr & 0xffff);

    // 打印指令指针、标志寄存器和堆栈指针信息
    printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
           esp[1], esp[0], esp[2], esp[4], esp[3]);

    // 打印 fs 段寄存器的值
    printk("fs: %04x\n", _fs());

    // 打印当前进程局部描述符表的基地址和限长
    printk("base: %p, limit: %p\n", get_base(current->ldt[1]), get_limit(0x17));

    // 如果堆栈段选择子为 0x17，打印堆栈信息
    if (esp[4] == 0x17) {
        printk("Stack: ");
        for (i = 0; i < 4; i++) {
            printk("%p ", get_seg_long(0x17, i + (long *)esp[3]));
        }
        printk("\n");
    }

    // 此处 str(i) 疑似代码有误，可能是笔误，原代码逻辑无法正常执行
    str(i);

    // 打印当前进程的 PID 和进程编号
    printk("Pid: %d, process nr: %d\n\r", current->pid, 0xffff & i);

    // 打印代码段的前 10 个字节
    for (i = 0; i < 10; i++) {
        printk("%02x ", 0xff & get_seg_byte(esp[1], (i + (char *)esp[0])));
    }
    printk("\n\r");

    // 以错误码 11 退出当前进程
    do_exit(11);        /* play segment exception */
}

void do_double_fault(long esp, long error_code)
{
    die("double fault",esp,error_code);
}

void do_general_protection(long esp, long error_code)
{
    die("general protection",esp,error_code);
}

void do_divide_error(long esp, long error_code)
{
    die("divide error", esp, error_code);
}

/**
 * @brief 处理 int3 中断（断点中断），打印寄存器信息
 * 
 * 该函数在 int3 中断发生时被调用，会打印多个寄存器的值，
 * 包括通用寄存器、段寄存器、指令指针、代码段选择子和标志寄存器等，
 * 用于调试目的。
 * 
 * @param esp 指向堆栈指针的指针，通过它可以获取中断现场的信息
 * @param error_code 中断错误码
 * @param fs fs 段寄存器的值
 * @param es es 段寄存器的值
 * @param ds ds 段寄存器的值
 * @param ebp ebp 寄存器的值
 * @param esi esi 寄存器的值
 * @param edi edi 寄存器的值
 * @param edx edx 寄存器的值
 * @param ecx ecx 寄存器的值
 * @param ebx ebx 寄存器的值
 * @param eax eax 寄存器的值
 */
void do_int3(long *esp, long error_code, long fs, long es, long ds,
             long ebp, long esi, long edi, long edx, long ecx, long ebx, long eax)
{
    int tr;

    // 获取任务寄存器（TR）的值
    __asm__("str %%ax"
            : "=a"(tr)
            : "0"(0));

    // 打印通用寄存器 eax, ebx, ecx, edx 的值
    printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
           eax, ebx, ecx, edx);

    // 打印通用寄存器 esi, edi, ebp 和堆栈指针的值
    printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
           esi, edi, ebp, (long)esp);

    // 打印段寄存器 ds, es, fs 和任务寄存器 tr 的值
    printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
           ds, es, fs, tr);

    // 打印指令指针、代码段选择子和标志寄存器的值
    printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r", esp[0], esp[1], esp[2]);
}

void do_nmi(long esp, long error_code)
{
    die("nmi",esp,error_code);
}

void do_debug(long esp, long error_code)
{
    die("debug",esp,error_code);
}

void do_overflow(long esp, long error_code)
{
    die("overflow",esp,error_code);
}

void do_bounds(long esp, long error_code)
{
    die("bounds",esp,error_code);
}

void do_invalid_op(long esp, long error_code)
{
    die("invalid operand",esp,error_code);
}

void do_device_not_available(long esp, long error_code)
{
    die("device not available",esp,error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
    die("coprocessor segment overrun",esp,error_code);
}

void do_invalid_TSS(long esp,long error_code)
{
    die("invalid TSS",esp,error_code);
}

void do_segment_not_present(long esp,long error_code)
{
    die("segment not present",esp,error_code);
}

void do_stack_segment(long esp,long error_code)
{
    die("stack segment",esp,error_code);
}

void do_coprocessor_error(long esp, long error_code)
{
    if (last_task_used_math != current)
        return;
    die("coprocessor error",esp,error_code);
}

void do_reserved(long esp, long error_code)
{
    die("reserved (15,17-47) error",esp,error_code);
}

/**
 * @brief 初始化中断陷阱门和系统门，配置 8259A 中断控制器
 * 
 * 此函数用于设置不同中断号对应的陷阱门和系统门，
 * 并对 8259A 中断控制器进行初始化操作，确保系统能正确处理各类中断。
 */
void trap_init(void)
{
    int i;

    // 设置陷阱门
    set_trap_gate(0, &divide_error);
    set_trap_gate(1, &debug);
    set_trap_gate(2, &nmi);

    // 设置系统门，int3 - 5 可被所有调用
    set_system_gate(3, &int3);
    set_system_gate(4, &overflow);
    set_system_gate(5, &bounds);

    // 继续设置陷阱门
    set_trap_gate(6, &invalid_op);
    set_trap_gate(7, &device_not_available);
    set_trap_gate(8, &double_fault);
    set_trap_gate(9, &coprocessor_segment_overrun);
    set_trap_gate(10, &invalid_TSS);
    set_trap_gate(11, &segment_not_present);
    set_trap_gate(12, &stack_segment);
    set_trap_gate(13, &general_protection);
    set_trap_gate(14, &page_fault);
    set_trap_gate(15, &reserved);
    set_trap_gate(16, &coprocessor_error);

    // 为 17 - 47 号中断设置陷阱门
    for (i = 17; i < 48; i++) {
        set_trap_gate(i, &reserved);
    }

    // 为 45 号中断设置陷阱门
    set_trap_gate(45, &irq13);

    // 操作 8259A 中断控制器
    outb_p(inb_p(0x21) & 0xfb, 0x21);
    outb(inb_p(0xA1) & 0xdf, 0xA1);

    // 为 39 号中断设置陷阱门
    set_trap_gate(39, &parallel_interrupt);
}
