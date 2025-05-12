/*
 *  linux/kernel/serial.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	serial.c
 *
 * This module implements the rs232 io functions
 *	void rs_write(struct tty_struct * queue);
 *	void rs_init(void);
 * and all interrupts pertaining to serial IO.
 */

#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

#define WAKEUP_CHARS (TTY_BUF_SIZE/4)

extern void rs1_interrupt(void);
extern void rs2_interrupt(void);

/**
 * @brief 初始化指定端口的串口设备
 * 
 * 此函数用于对指定端口的串口设备进行初始化配置，包括设置波特率、
 * 重置线路控制寄存器、设置控制信号以及使能中断等操作。
 * 
 * @param port 串口设备的端口地址
 */
static void init(int port)
{
    // 设置线路控制寄存器的 DLAB 位，允许访问波特率除数寄存器
    outb_p(0x80, port + 3);

    // 写入波特率除数寄存器的低字节（48 对应 2400 bps）
    outb_p(0x30, port);

    // 写入波特率除数寄存器的高字节
    outb_p(0x00, port + 1);

    // 重置线路控制寄存器的 DLAB 位
    outb_p(0x03, port + 3);

    // 设置 DTR、RTS 和 OUT_2 控制信号
    outb_p(0x0b, port + 4);

    // 使能除写操作之外的所有中断
    outb_p(0x0d, port + 1);

    // 读取数据端口以重置相关状态（原因不明）
    (void)inb(port);
}

/**
 * @brief 初始化串口设备
 * 
 * 此函数主要完成串口设备的初始化工作，包括设置中断门、
 * 调用底层初始化函数对串口进行配置，以及设置中断屏蔽寄存器。
 */
void rs_init(void)
{
    // 设置 0x24 号中断门，指向 rs1_interrupt 中断处理函数
    set_intr_gate(0x24, rs1_interrupt);
    // 设置 0x23 号中断门，指向 rs2_interrupt 中断处理函数
    set_intr_gate(0x23, rs2_interrupt);

    // 初始化第一个串口设备
    init(tty_table[1].read_q.data);
    // 初始化第二个串口设备
    init(tty_table[2].read_q.data);

    // 修改 8259A 中断控制器的中断屏蔽寄存器，允许串口 1 和 2 的中断
    outb(inb_p(0x21) & 0xE7, 0x21);
}

/*
 * This routine gets called when tty_write has put something into
 * the write_queue. It must check wheter the queue is empty, and
 * set the interrupt register accordingly
 *
 *	void _rs_write(struct tty_struct * tty);
 */
void rs_write(struct tty_struct * tty)
{
	cli();
	if (!EMPTY(tty->write_q))
		outb(inb_p(tty->write_q.data+1)|0x02,tty->write_q.data+1);
	sti();
}
