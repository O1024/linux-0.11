/*
 *  linux/kernel/panic.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This function is used through-out the kernel (includeinh mm and fs)
 * to indicate a major problem.
 */
#define PANIC

#include <linux/kernel.h>
#include <linux/sched.h>

void sys_sync(void);	/* it's really int */

/**
 * @brief 处理内核恐慌情况
 * 
 * 当内核遇到严重问题时调用此函数。它会打印内核恐慌信息，
 * 根据当前任务是否为 0 号任务决定是否进行同步操作，
 * 最后进入无限循环使系统挂起。
 * 
 * @param s 指向包含恐慌信息的字符串指针
 */
void panic(const char * s)
{
    // 打印内核恐慌信息
    printk("Kernel panic: %s\n\r", s);

    // 判断当前任务是否为 0 号任务
    if (current == task[0])
    {
        // 若为 0 号任务，打印提示信息且不同步
        printk("In swapper task - not syncing\n\r");
    }
    else
    {
        // 若不是 0 号任务，调用系统同步函数
        sys_sync();
    }

    // 进入无限循环，使系统挂起
    for (;;);
}
