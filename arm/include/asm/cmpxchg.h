#ifndef __ASM_ARM_CMPXCHG_H
#define __ASM_ARM_CMPXCHG_H

#include <linux/irqflags.h>
#include <asm/barrier.h>

#if defined(CONFIG_CPU_SA1100) || defined(CONFIG_CPU_SA110)
/*
 * On the StrongARM, "swp" is terminally broken since it bypasses the
 * cache totally.  This means that the cache becomes inconsistent, and,
 * since we use normal loads/stores as well, this is really bad.
 * Typically, this causes oopsen in filp_close, but could have other,
 * more disastrous effects.  There are two work-arounds:
 *  1. Disable interrupts and emulate the atomic swap
 *  2. Clean the cache, perform atomic swap, flush the cache
 *
 * We choose (1) since its the "easiest" to achieve here and is not
 * dependent on the processor type.
 *
 * NOTE that this solution won't work on an SMP system, so explcitly
 * forbid it here.
 */
#define swp_is_buggy
#endif

// ARM10C 20140315
// ((__typeof__(*&((&cpu_add_remove_lock->count)->counter))__xchg((unsigned long)(-1),
// (&lock->count))->counter),sizeof(*&((&cpu_add_remove_lock->count)->counter)))
//
// x: -1, ptr: (&cpu_add_remove_lock->count)->counter, size: 4
static inline unsigned long __xchg(unsigned long x, volatile void *ptr, int size)
{
	extern void __bad_xchg(volatile void *, int);
	unsigned long ret;
#ifdef swp_is_buggy
	unsigned long flags;
#endif
#if __LINUX_ARM_ARCH__ >= 6 // __LINUX_ARM_ARCH__: 7
	unsigned int tmp;
#endif

	smp_mb();

	// size: 4
	switch (size) {
#if __LINUX_ARM_ARCH__ >= 6 // __LINUX_ARM_ARCH__: 7
	case 1:
		asm volatile("@	__xchg1\n"
		"1:	ldrexb	%0, [%3]\n"
		"	strexb	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;
	case 4:
		// ptr: (&cpu_add_remove_lock->count)->counter: 1, x: -1
		// asm volatile("@	__xchg4\n"
		// "1:	ldrex	ret, [ptr]\n"
		// "	strex	tmp, x, [ptr]\n"
		// "	teq	tmp, ret\n"
		// "	bne	1b"
		asm volatile("@	__xchg4\n"
		"1:	ldrex	%0, [%3]\n"
		"	strex	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		// ret: 1, (&cpu_add_remove_lock->count)->counter: -1
		break;
#elif defined(swp_is_buggy)
#ifdef CONFIG_SMP
#error SMP is not supported on this platform
#endif
	case 1:
		raw_local_irq_save(flags);
		ret = *(volatile unsigned char *)ptr;
		*(volatile unsigned char *)ptr = x;
		raw_local_irq_restore(flags);
		break;

	case 4:
		raw_local_irq_save(flags);
		ret = *(volatile unsigned long *)ptr;
		*(volatile unsigned long *)ptr = x;
		raw_local_irq_restore(flags);
		break;
#else
	case 1:
		asm volatile("@	__xchg1\n"
		"	swpb	%0, %1, [%2]"
			: "=&r" (ret)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;
	case 4:
		asm volatile("@	__xchg4\n"
		"	swp	%0, %1, [%2]"
			: "=&r" (ret)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;
#endif
	default:
		__bad_xchg(ptr, size), ret = 0;
		break;
	}
	smp_mb();

	// ret: 1
	return ret;
	// return 1
}

// ARM10C 20140315
// xchg(&((&(&cpu_add_remove_lock)->count)->counter), -1):
// ((__typeof__(*(&((&(&cpu_add_remove_lock)->count)->counter))))__xchg((unsigned long)(-1),
// (&((&(&cpu_add_remove_lock)->count)->counter)),sizeof(*(&((&(&cpu_add_remove_lock)->count)->counter)))))
#define xchg(ptr,x)							\
	((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

#include <asm-generic/cmpxchg-local.h>

#if __LINUX_ARM_ARCH__ < 6 // __LINUX_ARM_ARCH__: 7
/* min ARCH < ARMv6 */

#ifdef CONFIG_SMP
#error "SMP is not supported on this platform"
#endif

/*
 * cmpxchg_local and cmpxchg64_local are atomic wrt current CPU. Always make
 * them available.
 */
#define cmpxchg_local(ptr, o, n)				  	       \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o),\
			(unsigned long)(n), sizeof(*(ptr))))
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#ifndef CONFIG_SMP
#include <asm-generic/cmpxchg.h>
#endif

#else	/* min ARCH >= ARMv6 */

extern void __bad_cmpxchg(volatile void *ptr, int size);

/*
 * cmpxchg only support 32-bits operands on ARMv6.
 */

// ARM10C 20161112
// ptr: &pid_ns->last_pid: &(&init_pid_ns)->last_pid, old: 0, new: 1, size: 4
static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long oldval, res;

	// size: 4
	switch (size) {
#ifndef CONFIG_CPU_V6	/* min ARCH >= ARMv6K */ // undefined
	case 1:
		do {
			asm volatile("@ __cmpxchg1\n"
			"	ldrexb	%1, [%2]\n"
			"	mov	%0, #0\n"
			"	teq	%1, %3\n"
			"	strexbeq %0, %4, [%2]\n"
				: "=&r" (res), "=&r" (oldval)
				: "r" (ptr), "Ir" (old), "r" (new)
				: "memory", "cc");
		} while (res);
		break;
	case 2:
		do {
			asm volatile("@ __cmpxchg1\n"
			"	ldrexh	%1, [%2]\n"
			"	mov	%0, #0\n"
			"	teq	%1, %3\n"
			"	strexheq %0, %4, [%2]\n"
				: "=&r" (res), "=&r" (oldval)
				: "r" (ptr), "Ir" (old), "r" (new)
				: "memory", "cc");
		} while (res);
		break;
#endif
	case 4:
		do {
			// ptr: &pid_ns->last_pid: &(&init_pid_ns)->last_pid, old: 0, new: 1
			asm volatile("@ __cmpxchg4\n"
			"	ldrex	%1, [%2]\n"
			"	mov	%0, #0\n"
			"	teq	%1, %3\n"
			"	strexeq %0, %4, [%2]\n"
				: "=&r" (res), "=&r" (oldval)
				: "r" (ptr), "Ir" (old), "r" (new)
				: "memory", "cc");
			// "@ __cmpxchg4\n"
			// " ldrex     oldval, [&(&init_pid_ns)->last_pid]\n"
			// " mov       res, #0\n"
			// " teq       oldval, 0\n"
			// " strexeq   res, 1, [&(&init_pid_ns)->last_pid]\n"
		} while (res);

		// 위 loop가 수행한 일:
		// &(&init_pid_ns)->last_pid 값을 atomic 하게 oldval 에 저장하고
		// &(&init_pid_ns)->last_pid 을 1 로 변경함, atomic 하게 무사히 변경시 res 값은 0 임
		// oldval: 0
		break;
	default:
		__bad_cmpxchg(ptr, size);
		oldval = 0;
	}

	// oldval: 0
	return oldval;
	// return 0
}

// ARM10C 20161112
// &pid_ns->last_pid: &(&init_pid_ns)->last_pid: 0, prev: 0, pid: 1, 4
static inline unsigned long __cmpxchg_mb(volatile void *ptr, unsigned long old,
					 unsigned long new, int size)
{
	unsigned long ret;

	smp_mb();

	// smp_mb 에서 한일:
	// 공유자원을 다른 cpu core가 사용할수 있게 해주는 옵션

	// ptr: &pid_ns->last_pid: &(&init_pid_ns)->last_pid, old: 0, new: 1, size: 4
	// __cmpxchg(&(&init_pid_ns)->last_pid, 0, 1, 4): 0
	ret = __cmpxchg(ptr, old, new, size);
	// ret: 0

	// __cmpxchg 이 한일:
	// &(&init_pid_ns)->last_pid 을 1 로 변경함

	smp_mb();

	// smp_mb 에서 한일:
	// 공유자원을 다른 cpu core가 사용할수 있게 해주는 옵션

	// ret: 0
	return ret;
	// return 0
}

// ARM10C 20161112
// &pid_ns->last_pid: &(&init_pid_ns)->last_pid: 0, prev: 0, pid: 1
#define cmpxchg(ptr,o,n)						\
	((__typeof__(*(ptr)))__cmpxchg_mb((ptr),			\
					  (unsigned long)(o),		\
					  (unsigned long)(n),		\
					  sizeof(*(ptr))))

static inline unsigned long __cmpxchg_local(volatile void *ptr,
					    unsigned long old,
					    unsigned long new, int size)
{
	unsigned long ret;

	switch (size) {
#ifdef CONFIG_CPU_V6	/* min ARCH == ARMv6 */
	case 1:
	case 2:
		ret = __cmpxchg_local_generic(ptr, old, new, size);
		break;
#endif
	default:
		ret = __cmpxchg(ptr, old, new, size);
	}

	return ret;
}

static inline unsigned long long __cmpxchg64(unsigned long long *ptr,
					     unsigned long long old,
					     unsigned long long new)
{
	unsigned long long oldval;
	unsigned long res;

	__asm__ __volatile__(
"1:	ldrexd		%1, %H1, [%3]\n"
"	teq		%1, %4\n"
"	teqeq		%H1, %H4\n"
"	bne		2f\n"
"	strexd		%0, %5, %H5, [%3]\n"
"	teq		%0, #0\n"
"	bne		1b\n"
"2:"
	: "=&r" (res), "=&r" (oldval), "+Qo" (*ptr)
	: "r" (ptr), "r" (old), "r" (new)
	: "cc");

	return oldval;
}

static inline unsigned long long __cmpxchg64_mb(unsigned long long *ptr,
						unsigned long long old,
						unsigned long long new)
{
	unsigned long long ret;

	smp_mb();
	ret = __cmpxchg64(ptr, old, new);
	smp_mb();

	return ret;
}

#define cmpxchg_local(ptr,o,n)						\
	((__typeof__(*(ptr)))__cmpxchg_local((ptr),			\
				       (unsigned long)(o),		\
				       (unsigned long)(n),		\
				       sizeof(*(ptr))))

#define cmpxchg64(ptr, o, n)						\
	((__typeof__(*(ptr)))__cmpxchg64_mb((ptr),			\
					(unsigned long long)(o),	\
					(unsigned long long)(n)))

#define cmpxchg64_relaxed(ptr, o, n)					\
	((__typeof__(*(ptr)))__cmpxchg64((ptr),				\
					(unsigned long long)(o),	\
					(unsigned long long)(n)))

#define cmpxchg64_local(ptr, o, n)	cmpxchg64_relaxed((ptr), (o), (n))

#endif	/* __LINUX_ARM_ARCH__ >= 6 */

#endif /* __ASM_ARM_CMPXCHG_H */
