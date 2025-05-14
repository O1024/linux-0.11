/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
static inline fork(void) __attribute__((always_inline));
static inline pause(void) __attribute__((always_inline));
static inline _syscall0(int, fork)
static inline _syscall0(int, pause)
static inline _syscall1(int, setup, void *,BIOS)
static inline _syscall0(int, sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)  // bootsect 重定向到 0x90000-90200 位置

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

/*
 * @brief 自定义的 printf 函数，用于格式化输出字符串到标准输出。
 *
 * 该函数使用 vsprintf 对可变参数进行格式化，将结果存储在 printbuf 中，
 * 然后通过 write 函数将格式化后的字符串输出到标准输出（文件描述符 1）。
 *
 * @param fmt 格式化字符串，包含普通文本和格式说明符。
 * @param ... 可变参数列表，对应格式化字符串中的格式说明符。
 * @return 成功时返回输出的字符数，发生错误时返回值可能不可靠。
 */
static int printf(const char *fmt, ...)
{
    // 定义一个 va_list 类型的变量，用于处理可变参数列表
    va_list args;
    // 用于存储格式化后的字符串长度
    int i;

    // 初始化可变参数列表，args 指向第一个可变参数
    va_start(args, fmt);
    // 使用 vsprintf 将可变参数按照 fmt 格式化为字符串存储在 printbuf 中，
    // 并将格式化后的字符串长度赋值给 i，然后使用 write 函数将其输出到标准输出
    write(1, printbuf, i = vsprintf(printbuf, fmt, args));
    // 清理可变参数列表，释放相关资源
    va_end(args);

    // 返回格式化输出的字符数
    return i;
}

/**
 * @brief 初始化系统时间
 * 
 * 此函数从 CMOS 实时时钟读取时间信息，将其从 BCD 码转换为二进制格式，
 * 并计算出系统启动时间。为确保读取的时间信息准确，会重复读取秒数，
 * 直到两次读取的秒数相同。
 */
static void time_init(void)
{
    struct tm time;

    // 重复读取时间信息，直到两次读取的秒数相同，确保时间信息的准确性
    do {
        time.tm_sec = CMOS_READ(0);   // 读取秒数
        time.tm_min = CMOS_READ(2);   // 读取分钟数
        time.tm_hour = CMOS_READ(4);  // 读取小时数
        time.tm_mday = CMOS_READ(7);  // 读取日期
        time.tm_mon = CMOS_READ(8);   // 读取月份
        time.tm_year = CMOS_READ(9);  // 读取年份
    } while (time.tm_sec != CMOS_READ(0));

    // 将 BCD 码表示的时间信息转换为二进制格式
    BCD_TO_BIN(time.tm_sec);
    BCD_TO_BIN(time.tm_min);
    BCD_TO_BIN(time.tm_hour);
    BCD_TO_BIN(time.tm_mday);
    BCD_TO_BIN(time.tm_mon);
    BCD_TO_BIN(time.tm_year);

    // 月份在 struct tm 中是从 0 开始计数的，因此减 1
    time.tm_mon--;

    // 计算系统启动时间
    startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

void main(void) /* This really IS void, no error here. */
{               /* The startup routine assumes (well, ...) this */
    /*
     * Interrupts are still disabled. Do necessary setups, then
     * enable them
     */

    ROOT_DEV = ORIG_ROOT_DEV;
    drive_info = DRIVE_INFO;
    memory_end = (1 << 20) + (EXT_MEM_K << 10);
    memory_end &= 0xfffff000;
    // 最大支持 16MB 内存
    if (memory_end > 16 * 1024 * 1024) {
        memory_end = 16 * 1024 * 1024;
    }

    if (memory_end > 12 * 1024 * 1024) {
        buffer_memory_end = 4 * 1024 * 1024;
    } else if (memory_end > 6 * 1024 * 1024) {
        buffer_memory_end = 2 * 1024 * 1024;
    } else {
        buffer_memory_end = 1 * 1024 * 1024;
    }
    main_memory_start = buffer_memory_end;

#ifdef RAMDISK
    main_memory_start += rd_init(main_memory_start, RAMDISK * 1024);
#endif
    // 划分内核区（0-1M）、buffer区（1-4M）和主内存区（4M-）
    mem_init(main_memory_start, memory_end);

    // 初始化中断陷阱门和系统门
    trap_init();

    // 初始化块设备请求队列
    blk_dev_init();

    // 初始化字符设备（打桩）
    chr_dev_init();

    // 初始化终端设备 serial 和 console 是物理概念，对应 uart 和 屏幕，tty 则是抽象概念
    tty_init();

    // 根据 CMOS 芯片存储的时间信息设置系统启动时间
    time_init();

    sched_init();

    buffer_init(buffer_memory_end);

    hd_init();

    floppy_init();

    // 开启 CPU 中断允许标志（如定时器中断、键盘中断等）
    sti();

    move_to_user_mode();

    if (!fork())    // 创建 task1 进程，pid 2，task1 执行 init() 函数
        init();

    /*
     *   NOTE!!   For any other task 'pause()' would mean we have to get a
     * signal to awaken, but task0 is the sole exception (see 'schedule()')
     * as task 0 gets activated at every idle moment (when no other tasks
     * can run). For task0 'pause()' just means we go check if some other
     * task can run, and if not we return here.
     */
    for (;;) {
        pause();
    }
}



static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
    int pid, i;

    setup((void *) &drive_info);
    (void) open("/dev/tty0", O_RDWR, 0);
    (void) dup(0);
    (void) dup(0);

    printf("%d buffers = %d bytes buffer space\n\r", NR_BUFFERS, NR_BUFFERS * BLOCK_SIZE);
    printf("Free mem: %d bytes\n\r", memory_end - main_memory_start);
    if (!(pid = fork())) {                  // 创建 task2 进程（pid = 3），执行 /etc/rc
        close(0);                           // 关闭标准输入
        if (open("/etc/rc", O_RDONLY, 0))   // 标准输入指向 /etc/rc
            _exit(1);
        execve("/bin/sh", argv_rc, envp_rc);// 当不带参数执行 /bin/sh 时，会从标准输入读取命令
        _exit(2);
    }

    // 等待 task2 进程结束
    if (pid > 0)
        while (pid != wait(&i));

    while (1) {
        if ((pid = fork()) < 0) {   // 创建 task3 进程，pid 4，shell 终端
            printf("Fork failed in init\r\n");
            continue;
        }
        if (!pid) {
            close(0);
            close(1);
            close(2);
            setsid();
            (void) open("/dev/tty0",O_RDWR,0);
            (void) dup(0);
            (void) dup(0);
            _exit(execve("/bin/sh",argv,envp));
        }

        while (1) {
            if (pid == wait(&i))
                break;
        }

        printf("\n\rchild %d died with code %04x\n\r", pid, i);

        sync();
    }
    _exit(0);    /* NOTE! _exit, not exit() */
}
