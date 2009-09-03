/*
 * Core fundamental timer interface
 */
#ifndef _CORE_JIFFIES_H
#define _CORE_JIFFIES_H

#define HZ	18

extern const volatile uint32_t __jiffies;
static inline uint32_t jiffies(void)
{
    return __jiffies;
}

#endif
