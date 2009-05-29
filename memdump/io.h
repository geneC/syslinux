#ifndef IO_H
#define IO_H

static inline void outb(unsigned char v, unsigned short p)
{
    asm volatile ("outb %1,%0"::"d" (p), "a"(v));
}

static inline unsigned char inb(unsigned short p)
{
    unsigned char v;
    asm volatile ("inb %1,%0":"=a" (v):"d"(p));
    return v;
}

static inline void outw(unsigned short v, unsigned short p)
{
    asm volatile ("outw %1,%0"::"d" (p), "a"(v));
}

static inline unsigned short inw(unsigned short p)
{
    unsigned short v;
    asm volatile ("inw %1,%0":"=a" (v):"d"(p));
    return v;
}

static inline void outl(unsigned int v, unsigned short p)
{
    asm volatile ("outl %1,%0"::"d" (p), "a"(v));
}

static inline unsigned int inl(unsigned short p)
{
    unsigned int v;
    asm volatile ("inl %1,%0":"=a" (v):"d"(p));
    return v;
}

#endif
