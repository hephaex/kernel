#ifndef __ASM_ARM_CPUTYPE_H
#define __ASM_ARM_CPUTYPE_H

#include <linux/stringify.h>
#include <linux/kernel.h>

#define CPUID_ID	0
#define CPUID_CACHETYPE	1
#define CPUID_TCM	2
#define CPUID_TLBTYPE	3
#define CPUID_MPUIR	4
// ARM10C 20140215
#define CPUID_MPIDR	5
#define CPUID_REVIDR	6

#ifdef CONFIG_CPU_V7M
#define CPUID_EXT_PFR0	0x40
#define CPUID_EXT_PFR1	0x44
#define CPUID_EXT_DFR0	0x48
#define CPUID_EXT_AFR0	0x4c
#define CPUID_EXT_MMFR0	0x50
#define CPUID_EXT_MMFR1	0x54
#define CPUID_EXT_MMFR2	0x58
#define CPUID_EXT_MMFR3	0x5c
#define CPUID_EXT_ISAR0	0x60
#define CPUID_EXT_ISAR1	0x64
#define CPUID_EXT_ISAR2	0x68
#define CPUID_EXT_ISAR3	0x6c
#define CPUID_EXT_ISAR4	0x70
#define CPUID_EXT_ISAR5	0x74
#else
#define CPUID_EXT_PFR0	"c1, 0"
#define CPUID_EXT_PFR1	"c1, 1"
#define CPUID_EXT_DFR0	"c1, 2"
#define CPUID_EXT_AFR0	"c1, 3"
#define CPUID_EXT_MMFR0	"c1, 4"
#define CPUID_EXT_MMFR1	"c1, 5"
#define CPUID_EXT_MMFR2	"c1, 6"
#define CPUID_EXT_MMFR3	"c1, 7"
#define CPUID_EXT_ISAR0	"c2, 0"
#define CPUID_EXT_ISAR1	"c2, 1"
#define CPUID_EXT_ISAR2	"c2, 2"
#define CPUID_EXT_ISAR3	"c2, 3"
#define CPUID_EXT_ISAR4	"c2, 4"
#define CPUID_EXT_ISAR5	"c2, 5"
#endif

#define MPIDR_SMP_BITMASK (0x3 << 30)
#define MPIDR_SMP_VALUE (0x2 << 30)

#define MPIDR_MT_BITMASK (0x1 << 24)

// ARM10C 20140215
#define MPIDR_HWID_BITMASK 0xFFFFFF

// ARM10C 20140215
// MPIDR_INVALID: 0xFF000000
#define MPIDR_INVALID (~MPIDR_HWID_BITMASK)

// ARM10C 20140215
#define MPIDR_LEVEL_BITS 8
// ARM10C 20140215
// MPIDR_LEVEL_MASK: 0xFF
#define MPIDR_LEVEL_MASK ((1 << MPIDR_LEVEL_BITS) - 1)

// ARM10C 20140215
// MPIDR_LEVEL_BITS 8, MPIDR_LEVEL_MASK: 0xFF
// #define MPIDR_AFFINITY_LEVEL(0x3, 0)
//	((0x3 >> (8 * 0)) & 0xFF)
// MPIDR_AFFINITY_LEVEL(0x3, 0): 0x3
#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	((mpidr >> (MPIDR_LEVEL_BITS * level)) & MPIDR_LEVEL_MASK)

#define ARM_CPU_IMP_ARM			0x41
#define ARM_CPU_IMP_INTEL		0x69

#define ARM_CPU_PART_ARM1136		0xB360
#define ARM_CPU_PART_ARM1156		0xB560
#define ARM_CPU_PART_ARM1176		0xB760
#define ARM_CPU_PART_ARM11MPCORE	0xB020
#define ARM_CPU_PART_CORTEX_A8		0xC080
// ARM10C 20140215
#define ARM_CPU_PART_CORTEX_A9		0xC090
#define ARM_CPU_PART_CORTEX_A5		0xC050
#define ARM_CPU_PART_CORTEX_A15		0xC0F0
#define ARM_CPU_PART_CORTEX_A7		0xC070

#define ARM_CPU_XSCALE_ARCH_MASK	0xe000
#define ARM_CPU_XSCALE_ARCH_V1		0x2000
#define ARM_CPU_XSCALE_ARCH_V2		0x4000
#define ARM_CPU_XSCALE_ARCH_V3		0x6000

extern unsigned int processor_id;

#ifdef CONFIG_CPU_CP15
// ARM10C 20130824
// 인라인 어셈블리 링크 참조
// http://wiki.kldp.org/wiki.php/DocbookSgml/GCC_Inline_Assembly-KLDP
// "cc" 의 의미는? 아래 링크 참조
// http://rootfriend.tistory.com/371
#define read_cpuid(reg)							\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, c0, " __stringify(reg)	\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})

/*
 * The memory clobber prevents gcc 4.5 from reordering the mrc before
 * any is_smp() tests, which can cause undefined instruction aborts on
 * ARM1136 r0 due to the missing extended CP15 registers.
 */
// ARM10C 20130914
// CPUID_EXT_ISAR0	"c2, 0"
#define read_cpuid_ext(ext_reg)						\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, " ext_reg		\
		    : "=r" (__val)					\
		    :							\
		    : "memory");					\
		__val;							\
	})

#elif defined(CONFIG_CPU_V7M)

#include <asm/io.h>
#include <asm/v7m.h>

#define read_cpuid(reg)							\
	({								\
		WARN_ON_ONCE(1);					\
		0;							\
	})

static inline unsigned int __attribute_const__ read_cpuid_ext(unsigned offset)
{
	return readl(BASEADDR_V7M_SCB + offset);
}

#else /* ifdef CONFIG_CPU_CP15 / elif defined (CONFIG_CPU_V7M) */

/*
 * read_cpuid and read_cpuid_ext should only ever be called on machines that
 * have cp15 so warn on other usages.
 */
#define read_cpuid(reg)							\
	({								\
		WARN_ON_ONCE(1);					\
		0;							\
	})

#define read_cpuid_ext(reg) read_cpuid(reg)

#endif /* ifdef CONFIG_CPU_CP15 / else */

// ARM10C 20130914
// CONFIG_CPU_CP15 = y
#ifdef CONFIG_CPU_CP15
/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant.  Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
// ARM10C 20130914 this
// ARM10C 20140215
static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
        // CPUID_ID: 0
	return read_cpuid(CPUID_ID);
        // read_cpuid(0): 0x413FC0F3
}

#elif defined(CONFIG_CPU_V7M)

static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return readl(BASEADDR_V7M_SCB + V7M_SCB_CPUID);
}

#else /* ifdef CONFIG_CPU_CP15 / elif defined(CONFIG_CPU_V7M) */

static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return processor_id;
}

#endif /* ifdef CONFIG_CPU_CP15 / else */

static inline unsigned int __attribute_const__ read_cpuid_implementor(void)
{
	return (read_cpuid_id() & 0xFF000000) >> 24;
}

// ARM10C 20140215
static inline unsigned int __attribute_const__ read_cpuid_part_number(void)
{
        // read_cpuid_id(): 0x413FC0F3
	return read_cpuid_id() & 0xFFF0;
        // return 0x0000C0F0
}

static inline unsigned int __attribute_const__ xscale_cpu_arch_version(void)
{
	return read_cpuid_part_number() & ARM_CPU_XSCALE_ARCH_MASK;
}

// ARM10C 20130914
static inline unsigned int __attribute_const__ read_cpuid_cachetype(void)
{
	return read_cpuid(CPUID_CACHETYPE);
}

static inline unsigned int __attribute_const__ read_cpuid_tcmstatus(void)
{
	return read_cpuid(CPUID_TCM);
}

// ARM10C 20130824
// __attribute_const__ 정보 아래링크 참조 
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0348bk/Cacgigch.html
// ARM10C 20140215
// A.R.M: B4.1.106 MPIDR, Multiprocessor Affinity Register, VMSA
static inline unsigned int __attribute_const__ read_cpuid_mpidr(void)
{
	return read_cpuid(CPUID_MPIDR);
}

/*
 * Intel's XScale3 core supports some v6 features (supersections, L2)
 * but advertises itself as v5 as it does not support the v6 ISA.  For
 * this reason, we need a way to explicitly test for this type of CPU.
 */
#ifndef CONFIG_CPU_XSC3
#define cpu_is_xsc3()	0
#else
static inline int cpu_is_xsc3(void)
{
	unsigned int id;
	id = read_cpuid_id() & 0xffffe000;
	/* It covers both Intel ID and Marvell ID */
	if ((id == 0x69056000) || (id == 0x56056000))
		return 1;

	return 0;
}
#endif

#if !defined(CONFIG_CPU_XSCALE) && !defined(CONFIG_CPU_XSC3)
#define	cpu_is_xscale()	0
#else
#define	cpu_is_xscale()	1
#endif

#endif
