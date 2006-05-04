#ifndef _CPU_H
#define _CPU_H

#include <inttypes.h>

static inline uint64_t rdtsc(void)
{
  uint64_t v;
  asm volatile("rdtsc" : "=A" (v));
  return v;
}

static inline uint32_t rdtscl(void)
{
  uint32_t v;
  asm volatile("rdtsc" : "=a" (v) : : "edx");
  return v;
}

static inline uint32_t cpuid_eax(uint32_t level)
{
  uint32_t v;

  asm("cpuid" : "=a" (v) : "a" (level) : "ebx", "ecx", "edx");
  return v;
}
static inline uint32_t cpuid_ebx(uint32_t level)
{
  uint32_t v;

  asm("cpuid" : "=b" (v),  "+a" (level) : : "ecx", "edx");
  return v;
}
static inline uint32_t cpuid_ecx(uint32_t level)
{
  uint32_t v;

  asm("cpuid" : "=c" (v), "+a" (level) : : "ebx", "edx");
  return v;
}
static inline uint32_t cpuid_edx(uint32_t level)
{
  uint32_t v;

  asm("cpuid" : "=d" (v), "+a" (level) : : "ebx", "ecx");
  return v;
}

static inline uint64_t rdmsr(uint32_t msr)
{
  uint64_t v;

  asm volatile("rdmsr" : "=A" (v) : "c" (msr));
  return v;
}
static inline void wrmsr(uint64_t v, uint32_t msr)
{
  asm volatile("wrmsr" : : "A" (v), "c" (msr));
}

static inline void cpu_relax(void)
{
  asm volatile("rep ; nop");
}

/* These are local cli/sti; not SMP-safe!!! */

static inline void cli(void)
{
  asm volatile("cli");
}

static inline void sti(void)
{
  asm volatile("sti");
}

#endif
