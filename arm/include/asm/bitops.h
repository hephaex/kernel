/*
 * Copyright 1995, Russell King.
 * Various bits and pieces copyrights include:
 *  Linus Torvalds (test_bit).
 * Big endian support: Copyright 2001, Nicolas Pitre
 *  reworked by rmk.
 *
 * bit 0 is the LSB of an "unsigned long" quantity.
 *
 * Please note that the code in this file should never be included
 * from user space.  Many of these are not implemented in assembler
 * since they would be too costly.  Also, they require privileged
 * instructions (which are not available from user mode) to ensure
 * that they are atomic.
 */

#ifndef __ASM_ARM_BITOPS_H
#define __ASM_ARM_BITOPS_H

#ifdef __KERNEL__

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <linux/irqflags.h>

#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	smp_mb()

/*
 * These functions are the basis of our bit ops.
 *
 * First, the atomic bitops. These use native endian.
 */
static inline void ____atomic_set_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	*p |= mask;
	raw_local_irq_restore(flags);
}

static inline void ____atomic_clear_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	*p &= ~mask;
	raw_local_irq_restore(flags);
}

static inline void ____atomic_change_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	*p ^= mask;
	raw_local_irq_restore(flags);
}

static inline int
____atomic_test_and_set_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	res = *p;
	*p = res | mask;
	raw_local_irq_restore(flags);

	return (res & mask) != 0;
}

static inline int
____atomic_test_and_clear_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	res = *p;
	*p = res & ~mask;
	raw_local_irq_restore(flags);

	return (res & mask) != 0;
}

static inline int
____atomic_test_and_change_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	res = *p;
	*p = res ^ mask;
	raw_local_irq_restore(flags);

	return (res & mask) != 0;
}

#include <asm-generic/bitops/non-atomic.h>

/*
 *  A note about Endian-ness.
 *  -------------------------
 *
 * When the ARM is put into big endian mode via CR15, the processor
 * merely swaps the order of bytes within words, thus:
 *
 *          ------------ physical data bus bits -----------
 *          D31 ... D24  D23 ... D16  D15 ... D8  D7 ... D0
 * little     byte 3       byte 2       byte 1      byte 0
 * big        byte 0       byte 1       byte 2      byte 3
 *
 * This means that reading a 32-bit word at address 0 returns the same
 * value irrespective of the endian mode bit.
 *
 * Peripheral devices should be connected with the data bus reversed in
 * "Big Endian" mode.  ARM Application Note 61 is applicable, and is
 * available from http://www.arm.com/.
 *
 * The following assumes that the data bus connectivity for big endian
 * mode has been followed.
 *
 * Note that bit 0 is defined to be 32-bit word bit 0, not byte 0 bit 0.
 */

/*
 * Native endian assembly bitops.  nr = 0 -> word 0 bit 0.
 */
extern void _set_bit(int nr, volatile unsigned long * p);
// ARM10C 20160903
extern void _clear_bit(int nr, volatile unsigned long * p);
extern void _change_bit(int nr, volatile unsigned long * p);
// ARM10C 20161112
extern int _test_and_set_bit(int nr, volatile unsigned long * p);
extern int _test_and_clear_bit(int nr, volatile unsigned long * p);
extern int _test_and_change_bit(int nr, volatile unsigned long * p);

/*
 * Little endian assembly bitops.  nr = 0 -> byte 0 bit 0.
 */
extern int _find_first_zero_bit_le(const void * p, unsigned size);
// ARM10C 20140607
// ARM10C 20141115
extern int _find_next_zero_bit_le(const void * p, int size, int offset);
extern int _find_first_bit_le(const unsigned long *p, unsigned size);
// ARM10C 20140607
// ARM10C 20141115
extern int _find_next_bit_le(const unsigned long *p, int size, int offset);

/*
 * Big endian assembly bitops.  nr = 0 -> byte 3 bit 0.
 */
extern int _find_first_zero_bit_be(const void * p, unsigned size);
extern int _find_next_zero_bit_be(const void * p, int size, int offset);
extern int _find_first_bit_be(const unsigned long *p, unsigned size);
extern int _find_next_bit_be(const unsigned long *p, int size, int offset);

#ifndef CONFIG_SMP // CONFIG_SMP=y
/*
 * The __* form of bitops are non-atomic and may be reordered.
 */
#define ATOMIC_BITOP(name,nr,p)			\
	(__builtin_constant_p(nr) ? ____atomic_##name(nr, p) : _##name(nr,p))
#else	// ARM10C Y
// ARM10C 20131207
// _test_and_clear_bit
// _test_and_set_bit
// ARM10C 20140621
// ARM10C 20160416
// ARM10C 20161112
#define ATOMIC_BITOP(name,nr,p)		_##name(nr,p)	// ARM10C this
#endif

/*
 * Native endian atomic definitions.
 */
// ARM10C 20141004
// 0, allocated_irqs
// ARM10C 20150509
// 0, &ts->check_clocks: [pcp0] &(&tick_cpu_sched)->check_clocks
#define set_bit(nr,p)			ATOMIC_BITOP(set_bit,nr,p)  // _set_bit(nr,p)로 치환
// ARM10C 20160903
// flag: 1, &ti->flags: &(((struct thread_info *)(할당 받은 page 2개의 메로리의 가상 주소))->flags
#define clear_bit(nr,p)			ATOMIC_BITOP(clear_bit,nr,p)  // _clear_bit(nr,p)로 치환
#define change_bit(nr,p)		ATOMIC_BITOP(change_bit,nr,p)
// ARM10C 20131207
// ARM10C 20140621
// bitnum: 0, addr: &(MIGRATE_UNMOVABLE인 page)->flags
// ARM10C 20160416
// 0, &once
// ARM10C 20161112
// offset: 1, map->page: (&(&init_pid_ns)->pidmap[0])->page: kmem_cache#25-oX
// ARM10C 20161126
// bitnum: 0, addr: hash 0xXXXXXXXX 에 맞는 list table 주소값
#define test_and_set_bit(nr,p)		ATOMIC_BITOP(test_and_set_bit,nr,p) // _test_and_set_bit(nr,p)
// ARM10C 20131207
#define test_and_clear_bit(nr,p)	ATOMIC_BITOP(test_and_clear_bit,nr,p)
#define test_and_change_bit(nr,p)	ATOMIC_BITOP(test_and_change_bit,nr,p)

#ifndef __ARMEB__
/*
 * These are the little endian, atomic definitions.
 */
#define find_first_zero_bit(p,sz)	_find_first_zero_bit_le(p,sz)
// ARM10C 20131207
// ARM10C 20140607
// chunk->populated: dchunk->populated[0]: 0xff, end: 0x4, *rs: 0x4
// ARM10C 20141115
// map: allocated_irqs, size: 8212, start: 16
// ARM10C 20141122
// map: allocated_irqs, size: 160, start: 16
// ARM10C 20141122
// map: allocated_irqs, size: 160, start: 32
// ARM10C 20141213
// map: allocated_irqs, size: 8212, start: 160
// ARM10C 20151107
// p->bitmap: (kmem_cache#21-o7)->bitmap (struct idr_layer), IDR_SIZE: 0x100, n: 0
// ARM10C 20151107
// bitmap->bitmap: (kmem_cache#27-oX (struct ida_bitmap))->bitmap, IDA_BITMAP_BITS: 992, offset: 0
// ARM10C 20160213
// p->bitmap: (kmem_cache#21-o7)->bitmap (struct idr_layer), IDR_SIZE: 0x100, n: 1
// ARM10C 20160305
// bitmap->bitmap: (kmem_cache#27-oX (struct ida_bitmap))->bitmap, IDA_BITMAP_BITS: 992, offset: 1
// ARM10C 20160416
// bitmap->bitmap: (kmem_cache#27-oX (struct ida_bitmap))->bitmap, IDA_BITMAP_BITS: 992, offset: 2
#define find_next_zero_bit(p,sz,off)	_find_next_zero_bit_le(p,sz,off)
#define find_first_bit(p,sz)		_find_first_bit_le(p,sz)
// ARM10C 20140215
// cpumask_bits(cpu_possible_mask): cpu_possible_mask->bits: 0xF , nr_cpumask_bits: 4, n: -1+1
// p: cpu_possible_mask->bits: 0xF , sz: 4, off: 0
// ARM10C 20140607
// chunk->populated: dchunk->populated[0]: 0xff, end: 0x4, *rs: 0x3
// ARM10C 20141115
// map: allocated_irqs, end: 160, index: 16
// ARM10C 20141213
// map: allocated_irqs, end: 416, index: 160
// ARM10C 20150523
// cpumask_bits(mask): mask->bits: 0x1, nr_cpumask_bits: 4, n: -1+1
#define find_next_bit(p,sz,off)		_find_next_bit_le(p,sz,off)

#else
/*
 * These are the big endian, atomic definitions.
 */
#define find_first_zero_bit(p,sz)	_find_first_zero_bit_be(p,sz)
#define find_next_zero_bit(p,sz,off)	_find_next_zero_bit_be(p,sz,off)
#define find_first_bit(p,sz)		_find_first_bit_be(p,sz)
#define find_next_bit(p,sz,off)		_find_next_bit_be(p,sz,off)

#endif

#if __LINUX_ARM_ARCH__ < 5

#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/ffs.h>

#else

static inline int constant_fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

/*
 * On ARMv5 and above those functions can be implemented around
 * the clz instruction for much better code efficiency.
 */

// ARM10C 20140215
// fls(0x3): 2
// ARM10C 20140222
// fls(0xf): 4
// ARM10C 20140322
// fls(3008) : 4096
// ARM10C 20140419
// fls(0x4): 3
// ARM10C 20141025
// fls(0x1000): 13
// ARM10C 20141206
// fls(511): 9
// ARM10C 20150110
// fls(0x30000): 18
// ARM10C 20151024
// size : 3
// ARM10C 20160827
static inline int fls(int x)
{
	int ret;

	if (__builtin_constant_p(x))
	       return constant_fls(x);

	// x: 0x3
	// asm("clz\t ret, x")
	asm("clz\t%0, %1" : "=r" (ret) : "r" (x));
	// ret: 30

       	ret = 32 - ret;
	// ret: 2

	return ret;
	// ret: 2
}

// ARM10C 20140222
#define __fls(x) (fls(x) - 1)

// ARM10C 20140215
// ffs(0x3):
// #define ffs(0x3) ({ unsigned long __t = (0x3); fls(0x3 & 0xFFFFFFFD); })
// fls(0x3 & 0xFFFFFFFD): fls(0x1): 1
// ffs(0x3): 1
// ffs: find frist set의 의미로 1로set된 최상위 bit의 bit index를 리턴
#define ffs(x) ({ unsigned long __t = (x); fls(__t & -__t); })
#define __ffs(x) (ffs(x) - 1)

// ARM10C 20140111
// find first zerobit
#define ffz(x) __ffs( ~(x) )

#endif

#include <asm-generic/bitops/fls64.h>

#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>

#ifdef __ARMEB__

static inline int find_first_zero_bit_le(const void *p, unsigned size)
{
	return _find_first_zero_bit_le(p, size);
}
#define find_first_zero_bit_le find_first_zero_bit_le

static inline int find_next_zero_bit_le(const void *p, int size, int offset)
{
	return _find_next_zero_bit_le(p, size, offset);
}
#define find_next_zero_bit_le find_next_zero_bit_le

static inline int find_next_bit_le(const void *p, int size, int offset)
{
	return _find_next_bit_le(p, size, offset);
}
#define find_next_bit_le find_next_bit_le

#endif

#include <asm-generic/bitops/le.h>

/*
 * Ext2 is defined to use little-endian byte ordering.
 */
#include <asm-generic/bitops/ext2-atomic-setbit.h>

#endif /* __KERNEL__ */

#endif /* _ARM_BITOPS_H */
