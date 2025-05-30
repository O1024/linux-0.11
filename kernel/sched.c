/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1<<((nr)-1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

#define LATCH (1193180 / HZ)        // 8253 定时器输入时钟脉冲为 1193180，1s 对应 1193180，因此 11931 对应 10ms

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies = 0;
long startup_time=0;
struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };     // 指向 user_stack 数组的末尾，段选择子为 0b00010 0 00，gdt 数据段 0级别
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i, next, c;
	struct task_struct **p;

	for(p = &LAST_TASK; p > &FIRST_TASK; --p) {
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {     // 检查任务的闹钟是否到期，alarm 表示任务到期的时钟节拍
                (*p)->signal |= (1 << (SIGALRM - 1));
                (*p)->alarm = 0;
			}
			// TASK_INTERRUPTIBLE 状态的任务接收到了未被阻塞的信号，将其唤醒，使其进入 TASK_RUNNING 状态
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) && (*p)->state == TASK_INTERRUPTIBLE)
                (*p)->state = TASK_RUNNING;
		}
    }

    // 调度器主循环
	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];

        // 遍历所有 TASK_RUNNING 状态的任务，找到时间片最大的任务
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c) {
                c = (*p)->counter;
                next = i;
            }
		}

        // 如果时间片最大的任务存在，则退出循环，执行任务切换
		if (c)
            break;
        
        // 更新任务的时间片计数器（长期占用 CPU 的进程减少时间片，等待久的进程增加时间片）
		for(p = &LAST_TASK; p > &FIRST_TASK; --p)
			if (*p) // 此时所有 TASK_RUNNING 状态的任务时间片均为 0，但是其他状态的任务时间片不是 0
				(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
	}
	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();

	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;               // 用户态程序运行时间
	else
		current->stime++;               // 内核态程序运行时间

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}

	if (current_DOR & 0xf0)
		do_floppy_timer();

	if ((--current->counter) > 0)       // 当前任务时间片未使用结束，返回继续执行
        return;
	current->counter = 0;               // 当前任务时间片使用结束，重置时间片计数器
	if (!cpl)                           // 当前任务为内核态，返回继续执行
        return;
	schedule();                         // 调度其他任务执行
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

/**
 * @brief 初始化系统调度相关设置
 * 
 * 此函数负责完成系统调度的初始化工作，包括检查信号处理结构体大小、
 * 设置 TSS 和 LDT 描述符、清空任务数组和 GDT 表项、清除 NT 标志位、
 * 设置任务寄存器和局部描述符表寄存器、配置 8253 定时器、设置定时器中断门和系统调用门等。
 */
void sched_init(void)
{
    int i;
    struct desc_struct * p;

    // 检查 struct sigaction 的大小是否为 16 字节，若不是则触发内核恐慌
    if (sizeof(struct sigaction) != 16)
        panic("Struct sigaction MUST be 16 bytes");

    // 设置第一个任务的 TSS 和 LDT 描述符（位于 gdt 区域）
    set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));
    set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));

    // 指向 GDT 表中第一个任务 TSS 和 LDT 描述符之后的位置
    p = gdt + 2 + FIRST_TSS_ENTRY;

    // 初始化任务数组和剩余 TSS LDT 表项
    for (i = 1; i < NR_TASKS; i++) {
        task[i] = NULL;
        // 清空 TSS 表项
        p->a = p->b = 0;
        p++;
        // 清空 LDT 表项
        p->a = p->b = 0;
        p++;
    }

    /* 清除 NT 标志位，避免后续出现问题 */
    __asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");

    // 加载任务寄存器
    ltr(0);
    // 加载局部描述符表寄存器
    lldt(0);

    // 配置 8253 定时器，二进制模式，模式 3，先写 LSB 后写 MSB，通道 0
    outb_p(0x36, 0x43);
    // 写入定时器计数值的低字节
    outb_p(LATCH & 0xff, 0x40);
    // 写入定时器计数值的高字节
    outb(LATCH >> 8, 0x40);

    // 设置 0x20 号中断门，指向定时器中断处理函数
    set_intr_gate(0x20, &timer_interrupt);
    // 允许 8259A 中断控制器的定时器中断
    outb(inb_p(0x21) & ~0x01, 0x21);

    // 设置 0x80 号系统调用门，指向系统调用处理函数
    set_system_gate(0x80, &system_call);
}
