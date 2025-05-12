/*
 *  linux/kernel/mktime.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <time.h>

/*
 * This isn't the library routine, it is only used in the kernel.
 * as such, we don't care about years<1970 etc, but assume everything
 * is ok. Similarly, TZ etc is happily ignored. We just do everything
 * as easily as possible. Let's find something public for the library
 * routines (although I think minix times is public).
 */
/*
 * PS. I hate whoever though up the year 1970 - couldn't they have gotten
 * a leap-year instead? I also hate Gregorius, pope or no. I'm grumpy.
 */
#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

/* interestingly, we assume leap-years */
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

/**
 * @brief 计算从 1970 年 1 月 1 日 00:00:00 开始的秒数
 * 
 * 该函数根据传入的 tm 结构体中的时间信息，计算从 1970 年 1 月 1 日 00:00:00 
 * 开始到该时间点所经过的秒数。此函数为内核使用，不考虑 1970 年之前的年份，
 * 也忽略时区等信息。
 * 
 * @param tm 指向 struct tm 结构体的指针，包含年、月、日、时、分、秒等时间信息
 * @return long 从 1970 年 1 月 1 日 00:00:00 开始的秒数
 */
long kernel_mktime(struct tm * tm)
{
    long res;
    int year;

    // 处理年份，将 tm->tm_year 转换为相对于 1970 年的年份
    if (tm->tm_year >= 70) {
        year = tm->tm_year - 70;
    } else {
        // Y2K 问题修复，处理 2000 年后的年份
        year = tm->tm_year + 100 - 70; 
    }

    // 计算从 1970 年开始的总秒数，考虑闰年的影响
    // YEAR*year 计算整年的秒数，DAY*((year+1)/4) 加上闰年多出来的天数对应的秒数
    res = YEAR * year + DAY * ((year + 1) / 4);

    // 加上当前月份之前的所有月份的总秒数
    res += month[tm->tm_mon];

    // 如果当前月份大于 1 且该年不是闰年，减去一天的秒数
    if (tm->tm_mon > 1 && ((year + 2) % 4)) {
        res -= DAY;
    }

    // 加上当前月中过去的天数对应的秒数
    res += DAY * (tm->tm_mday - 1);

    // 加上小时对应的秒数
    res += HOUR * tm->tm_hour;

    // 加上分钟对应的秒数
    res += MINUTE * tm->tm_min;

    // 加上秒数
    res += tm->tm_sec;

    return res;
}
