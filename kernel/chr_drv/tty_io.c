/*
 *  linux/kernel/tty_io.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'tty_io.c' gives an orthogonal feeling to tty's, be they consoles
 * or rs-channels. It also implements echoing, cooked mode etc.
 *
 * Kill-line thanks to John T Kohl.
 */
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#define ALRMMASK (1<<(SIGALRM-1))
#define KILLMASK (1<<(SIGKILL-1))
#define INTMASK (1<<(SIGINT-1))
#define QUITMASK (1<<(SIGQUIT-1))
#define TSTPMASK (1<<(SIGTSTP-1))

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/segment.h>
#include <asm/system.h>

#define _L_FLAG(tty,f)    ((tty)->termios.c_lflag & f)
#define _I_FLAG(tty,f)    ((tty)->termios.c_iflag & f)
#define _O_FLAG(tty,f)    ((tty)->termios.c_oflag & f)

#define L_CANON(tty)    _L_FLAG((tty),ICANON)
#define L_ISIG(tty)    _L_FLAG((tty),ISIG)
#define L_ECHO(tty)    _L_FLAG((tty),ECHO)
#define L_ECHOE(tty)    _L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)    _L_FLAG((tty),ECHOK)
#define L_ECHOCTL(tty)    _L_FLAG((tty),ECHOCTL)
#define L_ECHOKE(tty)    _L_FLAG((tty),ECHOKE)

#define I_UCLC(tty)    _I_FLAG((tty),IUCLC)
#define I_NLCR(tty)    _I_FLAG((tty),INLCR)
#define I_CRNL(tty)    _I_FLAG((tty),ICRNL)
#define I_NOCR(tty)    _I_FLAG((tty),IGNCR)

#define O_POST(tty)    _O_FLAG((tty),OPOST)
#define O_NLCR(tty)    _O_FLAG((tty),ONLCR)
#define O_CRNL(tty)    _O_FLAG((tty),OCRNL)
#define O_NLRET(tty)    _O_FLAG((tty),ONLRET)
#define O_LCUC(tty)    _O_FLAG((tty),OLCUC)

/**
 * @brief 定义终端设备结构体数组，包含控制台和两个串口设备信息
 * 
 * 此数组存储了控制台和两个串口设备的终端配置信息，
 * 包括终端属性、进程组 ID、停止状态、写函数指针以及不同用途的队列。
 */
struct tty_struct tty_table[] = {
    {
        // 控制台终端属性
        {
            ICRNL,          /* 将输入的 CR 转换为 NL */
            OPOST | ONLCR,  /* 将输出的 NL 转换为 CRNL */
            0,
            ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
            0,              /* 控制台 termio */
            INIT_C_CC
        },
        0,                  /* 初始进程组 ID */
        0,                  /* 初始停止状态 */
        con_write,          /* 控制台写函数 */
        {0, 0, 0, 0, ""},   /* 控制台读队列 */
        {0, 0, 0, 0, ""},   /* 控制台写队列 */
        {0, 0, 0, 0, ""}    /* 控制台辅助队列 */
    },
    {
        // 串口 1 终端属性
        {
            0,              /* 无输入转换 */
            0,              /* 无输出转换 */
            B2400 | CS8,
            0,
            0,
            INIT_C_CC
        },
        0,                  /* 初始进程组 ID */
        0,                  /* 初始停止状态 */
        rs_write,           /* 串口写函数 */
        {0x3f8, 0, 0, 0, ""},/* 串口 1 读队列 */
        {0x3f8, 0, 0, 0, ""},/* 串口 1 写队列 */
        {0, 0, 0, 0, ""}    /* 串口 1 辅助队列 */
    },
    {
        // 串口 2 终端属性
        {
            0,              /* 无输入转换 */
            0,              /* 无输出转换 */
            B2400 | CS8,
            0,
            0,
            INIT_C_CC
        },
        0,                  /* 初始进程组 ID */
        0,                  /* 初始停止状态 */
        rs_write,           /* 串口写函数 */
        {0x2f8, 0, 0, 0, ""},/* 串口 2 读队列 */
        {0x2f8, 0, 0, 0, ""},/* 串口 2 写队列 */
        {0, 0, 0, 0, ""}    /* 串口 2 辅助队列 */
    }
};

/*
 * these are the tables used by the machine code handlers.
 * you can implement pseudo-tty's or something by changing
 * them. Currently not done.
 */
struct tty_queue * table_list[]={
    &tty_table[0].read_q, &tty_table[0].write_q,
    &tty_table[1].read_q, &tty_table[1].write_q,
    &tty_table[2].read_q, &tty_table[2].write_q
    };

/**
 * @brief 初始化终端设备
 * 
 * 该函数负责调用 rs_init() 和 con_init() 函数，
 * 分别对串口终端和控制台终端进行初始化操作。
 */
void tty_init(void)
{
    // 初始化串口设备
    rs_init();
    con_init();
}

void tty_intr(struct tty_struct * tty, int mask)
{
    int i;

    if (tty->pgrp <= 0)
        return;
    for (i=0;i<NR_TASKS;i++)
        if (task[i] && task[i]->pgrp==tty->pgrp)
            task[i]->signal |= mask;
}

static void sleep_if_empty(struct tty_queue * queue)
{
    cli();
    while (!current->signal && EMPTY(*queue))
        interruptible_sleep_on(&queue->proc_list);
    sti();
}

static void sleep_if_full(struct tty_queue * queue)
{
    if (!FULL(*queue))
        return;
    cli();
    while (!current->signal && LEFT(*queue)<128)
        interruptible_sleep_on(&queue->proc_list);
    sti();
}

void wait_for_keypress(void)
{
    sleep_if_empty(&tty_table[0].secondary);
}

void copy_to_cooked(struct tty_struct * tty)
{
    signed char c;

    while (!EMPTY(tty->read_q) && !FULL(tty->secondary)) {
        GETCH(tty->read_q,c);
        if (c==13)
            if (I_CRNL(tty))
                c=10;
            else if (I_NOCR(tty))
                continue;
            else ;
        else if (c==10 && I_NLCR(tty))
            c=13;
        if (I_UCLC(tty))
            c=tolower(c);
        if (L_CANON(tty)) {
            if (c==KILL_CHAR(tty)) {
                /* deal with killing the input line */
                while(!(EMPTY(tty->secondary) ||
                        (c=LAST(tty->secondary))==10 ||
                        c==EOF_CHAR(tty))) {
                    if (L_ECHO(tty)) {
                        if (c<32)
                            PUTCH(127,tty->write_q);
                        PUTCH(127,tty->write_q);
                        tty->write(tty);
                    }
                    DEC(tty->secondary.head);
                }
                continue;
            }
            if (c==ERASE_CHAR(tty)) {
                if (EMPTY(tty->secondary) ||
                   (c=LAST(tty->secondary))==10 ||
                   c==EOF_CHAR(tty))
                    continue;
                if (L_ECHO(tty)) {
                    if (c<32)
                        PUTCH(127,tty->write_q);
                    PUTCH(127,tty->write_q);
                    tty->write(tty);
                }
                DEC(tty->secondary.head);
                continue;
            }
            if (c==STOP_CHAR(tty)) {
                tty->stopped=1;
                continue;
            }
            if (c==START_CHAR(tty)) {
                tty->stopped=0;
                continue;
            }
        }
        if (L_ISIG(tty)) {
            if (c==INTR_CHAR(tty)) {
                tty_intr(tty,INTMASK);
                continue;
            }
            if (c==QUIT_CHAR(tty)) {
                tty_intr(tty,QUITMASK);
                continue;
            }
        }
        if (c==10 || c==EOF_CHAR(tty))
            tty->secondary.data++;
        if (L_ECHO(tty)) {
            if (c==10) {
                PUTCH(10,tty->write_q);
                PUTCH(13,tty->write_q);
            } else if (c<32) {
                if (L_ECHOCTL(tty)) {
                    PUTCH('^',tty->write_q);
                    PUTCH(c+64,tty->write_q);
                }
            } else
                PUTCH(c,tty->write_q);
            tty->write(tty);
        }
        PUTCH(c,tty->secondary);
    }
    wake_up(&tty->secondary.proc_list);
}

int tty_read(unsigned channel, char * buf, int nr)
{
    struct tty_struct * tty;
    char c, * b=buf;
    int minimum,time,flag=0;
    long oldalarm;

    if (channel>2 || nr<0) return -1;
    tty = &tty_table[channel];
    oldalarm = current->alarm;
    time = 10L*tty->termios.c_cc[VTIME];
    minimum = tty->termios.c_cc[VMIN];
    if (time && !minimum) {
        minimum=1;
        if ((flag=(!oldalarm || time+jiffies<oldalarm)))
            current->alarm = time+jiffies;
    }
    if (minimum>nr)
        minimum=nr;
    while (nr>0) {
        if (flag && (current->signal & ALRMMASK)) {
            current->signal &= ~ALRMMASK;
            break;
        }
        if (current->signal)
            break;
        if (EMPTY(tty->secondary) || (L_CANON(tty) &&
        !tty->secondary.data && LEFT(tty->secondary)>20)) {
            sleep_if_empty(&tty->secondary);
            continue;
        }
        do {
            GETCH(tty->secondary,c);
            if (c==EOF_CHAR(tty) || c==10)
                tty->secondary.data--;
            if (c==EOF_CHAR(tty) && L_CANON(tty))
                return (b-buf);
            else {
                put_fs_byte(c,b++);
                if (!--nr)
                    break;
            }
        } while (nr>0 && !EMPTY(tty->secondary));
        if (time && !L_CANON(tty)) {
            if ((flag=(!oldalarm || time+jiffies<oldalarm)))
                current->alarm = time+jiffies;
            else
                current->alarm = oldalarm;
        }
        if (L_CANON(tty)) {
            if (b-buf)
                break;
        } else if (b-buf >= minimum)
            break;
    }
    current->alarm = oldalarm;
    if (current->signal && !(b-buf))
        return -EINTR;
    return (b-buf);
}

/**
 * @brief 向指定终端设备写入数据
 * 
 * 该函数将指定缓冲区中的数据写入到指定的终端设备。
 * 在写入过程中，会根据终端的输出模式对数据进行转换，
 * 若写队列已满则会让当前任务休眠，若收到信号则会中断写入。
 * 
 * @param channel 终端设备通道号，取值范围为 0 - 2
 * @param buf 指向要写入数据的缓冲区指针
 * @param nr 要写入的字节数
 * @return int 实际写入的字节数，若参数无效则返回 -1
 */
int tty_write(unsigned channel, char * buf, int nr)
{
    static int cr_flag = 0;
    struct tty_struct * tty;
    char c, *b = buf;

    // 检查通道号和写入字节数是否有效，无效则返回 -1
    if (channel > 2 || nr < 0)
        return -1;

    // 获取对应通道的终端设备结构体指针
    tty = &tty_table[channel];

    // 循环写入数据，直到写入指定字节数或遇到中断
    while (nr > 0)
    {
        // 若写队列已满，当前任务休眠等待
        sleep_if_full(&tty->write_q);

        // 若收到信号，中断写入
        if (current->signal)
            break;

        // 在队列未满且还有数据要写入时，继续写入操作
        while (nr > 0 && !FULL(tty->write_q))
        {
            // 从用户空间读取一个字节数据
            c = get_fs_byte(b);

            // 若终端开启输出处理，对数据进行转换
            if (O_POST(tty))
            {
                // 将回车符转换为换行符
                if (c == '\r' && O_CRNL(tty))
                    c = '\n';
                // 将换行符转换为回车符
                else if (c == '\n' && O_NLRET(tty))
                    c = '\r';

                // 在换行且未设置 cr_flag 时，先写入回车符
                if (c == '\n' && !cr_flag && O_NLCR(tty))
                {
                    cr_flag = 1;
                    PUTCH(13, tty->write_q);
                    continue;
                }

                // 将小写字母转换为大写字母
                if (O_LCUC(tty))
                    c = toupper(c);
            }

            // 移动缓冲区指针，减少剩余写入字节数
            b++;
            nr--;

            // 重置 cr_flag
            cr_flag = 0;

            // 将处理后的数据放入写队列
            PUTCH(c, tty->write_q);
        }

        // 调用终端写函数将数据输出
        tty->write(tty);

        // 若还有数据未写入，调度其他任务
        if (nr > 0)
            schedule();
    }

    // 返回实际写入的字节数
    return (b - buf);
}

/*
 * Jeh, sometimes I really like the 386.
 * This routine is called from an interrupt,
 * and there should be absolutely no problem
 * with sleeping even in an interrupt (I hope).
 * Of course, if somebody proves me wrong, I'll
 * hate intel for all time :-). We'll have to
 * be careful and see to reinstating the interrupt
 * chips before calling this, though.
 *
 * I don't think we sleep here under normal circumstances
 * anyway, which is good, as the task sleeping might be
 * totally innocent.
 */
void do_tty_interrupt(int tty)
{
    copy_to_cooked(tty_table+tty);
}

/**
 * @brief 初始化字符设备
 * 
 * 此函数目前为空，可能后续会添加字符设备初始化相关的代码。
 */
void chr_dev_init(void)
{
}
