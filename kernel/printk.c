/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

/**
 * @brief 内核模式下的格式化输出函数
 * 
 * 该函数类似于标准库的 printf 函数，用于在内核模式下进行格式化输出。
 * 由于内核模式下不能直接使用 printf，此函数先将格式化后的字符串存入缓冲区，
 * 然后通过调用 tty_write 函数将缓冲区内容输出到终端。
 * 
 * @param fmt 格式化字符串，与 printf 函数的第一个参数类似
 * @param ... 可变参数列表，对应格式化字符串中的占位符
 * @return int 输出字符串的长度
 */
int printk(const char *fmt, ...)
{
    va_list args;
    int i;

    // 初始化可变参数列表
    va_start(args, fmt);
    // 使用 vsprintf 将格式化后的字符串存入 buf 缓冲区
    i = vsprintf(buf, fmt, args);
    // 结束可变参数列表的使用
    va_end(args);

    // 使用内联汇编调用 tty_write 函数将缓冲区内容输出到终端
    __asm__ (
        "push %%fs\n\t"
        "push %%ds\n\t"
        "pop %%fs\n\t"
        "pushl %0\n\t"
        "pushl $buf\n\t"
        "pushl $0\n\t"
        "call tty_write\n\t"
        "addl $8,%%esp\n\t"
        "popl %0\n\t"
        "pop %%fs"
        ::"r" (i)
        :"ax", "cx", "dx"
    );

    return i;
}
