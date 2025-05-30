#define move_to_user_mode() \
__asm__ ("movl %%esp,%%eax\n\t" \
    "pushl $0x17\n\t" \
    "pushl %%eax\n\t" \
    "pushfl\n\t" \
    "pushl $0x0f\n\t" \
    "pushl $1f\n\t" \
    "iret\n" \
    "1:\tmovl $0x17,%%eax\n\t" \
    "movw %%ax,%%ds\n\t" \
    "movw %%ax,%%es\n\t" \
    "movw %%ax,%%fs\n\t" \
    "movw %%ax,%%gs" \
    :::"ax")

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

#define _set_gate(gate_addr, type, dpl, addr) \
    __asm__ ("movw %%dx,%%ax\n\t" \
             "movw %0,%%dx\n\t" \
             "movl %%eax,%1\n\t" \
             "movl %%edx,%2" \
             : \
             : "i" ((short) (0x8000 + (dpl << 13) + (type << 8))), \
               "o" (*((char *) (gate_addr))), \
               "o" (*(4 + (char *) (gate_addr))), \
               "d" ((char *) (addr)), \
               "a" (0x00080000))

/**
 * @brief 设置中断门描述符
 * 
 * 该宏用于设置中断门描述符，中断门用于处理中断请求。
 * 
 * @param n 中断描述符表（IDT）中的索引
 * @param addr 中断处理函数的地址
 */
#define set_intr_gate(n, addr) \
    _set_gate(&idt[n], 14, 0, addr)

/**
 * @brief 设置陷阱门描述符
 * 
 * 该宏用于设置陷阱门描述符，陷阱门用于处理异常。
 * 
 * @param n 中断描述符表（IDT）中的索引
 * @param addr 异常处理函数的地址
 */
#define set_trap_gate(n, addr) \
    _set_gate(&idt[n], 15, 0, addr)

/**
 * @brief 设置系统门描述符
 * 
 * 该宏用于设置系统门描述符，系统门允许用户态程序调用特定的内核服务。
 * 
 * @param n 中断描述符表（IDT）中的索引
 * @param addr 系统服务函数的地址
 */
#define set_system_gate(n, addr) \
    _set_gate(&idt[n], 15, 3, addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
    *(gate_addr) = ((base) & 0xff000000) | \
        (((base) & 0x00ff0000)>>16) | \
        ((limit) & 0xf0000) | \
        ((dpl)<<13) | \
        (0x00408000) | \
        ((type)<<8); \
    *((gate_addr)+1) = (((base) & 0x0000ffff)<<16) | \
        ((limit) & 0x0ffff); }

/**
 * @brief 设置 TSS 或 LDT 描述符
 * 
 * 该宏用于设置任务状态段（TSS）或局部描述符表（LDT）的描述符。
 * 它将相关信息写入指定的内存位置，以完成描述符的初始化。
 * 
 * @param n 指向描述符存储位置的指针
 * @param addr TSS 或 LDT 的基地址
 * @param type 描述符类型，不同的类型对应 TSS 或 LDT
 */
#define _set_tssldt_desc(n, addr, type) \
__asm__ ("movw $104,%1\n\t" \
         "movw %%ax,%2\n\t" \
         "rorl $16,%%eax\n\t" \
         "movb %%al,%3\n\t" \
         "movb $" type ",%4\n\t" \
         "movb $0x00,%5\n\t" \
         "movb %%ah,%6\n\t" \
         "rorl $16,%%eax" \
         ::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
         "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
        )

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")

