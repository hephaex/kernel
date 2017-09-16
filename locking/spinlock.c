﻿/*
 * Copyright (2004) Linus Torvalds
 *
 * Author: Zwane Mwaikambo <zwane@fsmlabs.com>
 *
 * Copyright (2004, 2005) Ingo Molnar
 *
 * This file contains the spinlock/rwlock implementations for the
 * SMP and the DEBUG_SPINLOCK cases. (UP-nondebug inlines them)
 *
 * Note that some architectures have special knowledge about the
 * stack frames of these functions in their profile_pc. If you
 * change anything significant here that could change the stack
 * frame contact the architecture maintainers.
 */

#include <linux/linkage.h>
#include <linux/preempt.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/export.h>

/*
 * If lockdep is enabled then we use the non-preemption spin-ops
 * even on CONFIG_PREEMPT, because lockdep assumes that interrupts are
 * not re-enabled during lock-acquire (which the preempt-spin-ops do):
 */
#if !defined(CONFIG_GENERIC_LOCKBREAK) || defined(CONFIG_DEBUG_LOCK_ALLOC)
/*
 * The __lock_function inlines are taken from
 * include/linux/spinlock_api_smp.h
 */
#else
#define raw_read_can_lock(l)	read_can_lock(l)
#define raw_write_can_lock(l)	write_can_lock(l)

/*
 * Some architectures can relax in favour of the CPU owning the lock.
 */
#ifndef arch_read_relax
# define arch_read_relax(l)	cpu_relax()
#endif
#ifndef arch_write_relax
# define arch_write_relax(l)	cpu_relax()
#endif
#ifndef arch_spin_relax
# define arch_spin_relax(l)	cpu_relax()
#endif

/*
 * We build the __lock_function inlines here. They are too large for
 * inlining all over the place, but here is only one user per function
 * which embedds them into the calling _lock_function below.
 *
 * This could be a long-held lock. We both prepare to spin for a long
 * time (making _this_ CPU preemptable if possible), and we also signal
 * towards that other CPU that it should break the lock ASAP.
 */
#define BUILD_LOCK_OPS(op, locktype)					\
void __lockfunc __raw_##op##_lock(locktype##_t *lock)			\
{									\
	for (;;) {							\
		preempt_disable();					\
		if (likely(do_raw_##op##_trylock(lock)))		\
			break;						\
		preempt_enable();					\
									\
		if (!(lock)->break_lock)				\
			(lock)->break_lock = 1;				\
		while (!raw_##op##_can_lock(lock) && (lock)->break_lock)\
			arch_##op##_relax(&lock->raw_lock);		\
	}								\
	(lock)->break_lock = 0;						\
}									\
									\
unsigned long __lockfunc __raw_##op##_lock_irqsave(locktype##_t *lock)	\
{									\
	unsigned long flags;						\
									\
	for (;;) {							\
		preempt_disable();					\
		local_irq_save(flags);					\
		if (likely(do_raw_##op##_trylock(lock)))		\
			break;						\
		local_irq_restore(flags);				\
		preempt_enable();					\
									\
		if (!(lock)->break_lock)				\
			(lock)->break_lock = 1;				\
		while (!raw_##op##_can_lock(lock) && (lock)->break_lock)\
			arch_##op##_relax(&lock->raw_lock);		\
	}								\
	(lock)->break_lock = 0;						\
	return flags;							\
}									\
									\
void __lockfunc __raw_##op##_lock_irq(locktype##_t *lock)		\
{									\
	_raw_##op##_lock_irqsave(lock);					\
}									\
									\
void __lockfunc __raw_##op##_lock_bh(locktype##_t *lock)		\
{									\
	unsigned long flags;						\
									\
	/*							*/	\
	/* Careful: we must exclude softirqs too, hence the	*/	\
	/* irq-disabling. We use the generic preemption-aware	*/	\
	/* function:						*/	\
	/**/								\
	flags = _raw_##op##_lock_irqsave(lock);				\
	local_bh_disable();						\
	local_irq_restore(flags);					\
}									\

/*
 * Build preemption-friendly versions of the following
 * lock-spinning functions:
 *
 *         __[spin|read|write]_lock()
 *         __[spin|read|write]_lock_irq()
 *         __[spin|read|write]_lock_irqsave()
 *         __[spin|read|write]_lock_bh()
 */
BUILD_LOCK_OPS(spin, raw_spinlock);
BUILD_LOCK_OPS(read, rwlock);
BUILD_LOCK_OPS(write, rwlock);

#endif

#ifndef CONFIG_INLINE_SPIN_TRYLOCK  // ARM10C Y 
int __lockfunc _raw_spin_trylock(raw_spinlock_t *lock)
{
	return __raw_spin_trylock(lock);
}
EXPORT_SYMBOL(_raw_spin_trylock);
#endif

#ifndef CONFIG_INLINE_SPIN_TRYLOCK_BH
int __lockfunc _raw_spin_trylock_bh(raw_spinlock_t *lock)
{
	return __raw_spin_trylock_bh(lock);
}
EXPORT_SYMBOL(_raw_spin_trylock_bh);
#endif

#ifndef CONFIG_INLINE_SPIN_LOCK // CONFIG_INLINE_SPIN_LOCK=n
// ARM10C 20140405
// ARM10C 20140517
// &lock->rlock: &(&contig_page_data->node_zones[0].lock)->rlock
// ARM10C 20160723
// &(&(kmem_cache#29-oX (struct freezer))->lock)->rlock
// ARM10C 20170201
// &rq->lock: [pcp0] &(&runqueues)->lock
void __lockfunc _raw_spin_lock(raw_spinlock_t *lock)
{
	__raw_spin_lock(lock);
}
EXPORT_SYMBOL(_raw_spin_lock);
#endif

#ifndef CONFIG_INLINE_SPIN_LOCK_IRQSAVE
unsigned long __lockfunc _raw_spin_lock_irqsave(raw_spinlock_t *lock)
{
	return __raw_spin_lock_irqsave(lock);
}
EXPORT_SYMBOL(_raw_spin_lock_irqsave);
#endif

#ifndef CONFIG_INLINE_SPIN_LOCK_IRQ // CONFIG_INLINE_SPIN_LOCK_IRQ=n
// ARM10C 20160514
// &lock->rlock: &(&proc_inum_lock)->rlock
void __lockfunc _raw_spin_lock_irq(raw_spinlock_t *lock)
{
	// lock: &(&proc_inum_lock)->rlock
	__raw_spin_lock_irq(lock);
}
EXPORT_SYMBOL(_raw_spin_lock_irq);
#endif

#ifndef CONFIG_INLINE_SPIN_LOCK_BH
void __lockfunc _raw_spin_lock_bh(raw_spinlock_t *lock)
{
	__raw_spin_lock_bh(lock);
}
EXPORT_SYMBOL(_raw_spin_lock_bh);
#endif

#ifdef CONFIG_UNINLINE_SPIN_UNLOCK // CONFIG_UNINLINE_SPIN_UNLOCK=y
// ARM10C 20140412
// ARM10C 20170520
void __lockfunc _raw_spin_unlock(raw_spinlock_t *lock)
{
	__raw_spin_unlock(lock);
}
EXPORT_SYMBOL(_raw_spin_unlock);
#endif

// ARM10C 20130907 
#ifndef CONFIG_INLINE_SPIN_UNLOCK_IRQRESTORE
void __lockfunc _raw_spin_unlock_irqrestore(raw_spinlock_t *lock, unsigned long flags)
{
	__raw_spin_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(_raw_spin_unlock_irqrestore);
#endif

#ifndef CONFIG_INLINE_SPIN_UNLOCK_IRQ // CONFIG_INLINE_SPIN_UNLOCK_IRQ=n
// ARM10C 20160514
// &lock->rlock: &(&proc_inum_lock)->rlock
// ARM10C 20170830
// &rq->lock: [pcp0] &(&runqueues)->lock
void __lockfunc _raw_spin_unlock_irq(raw_spinlock_t *lock)
{
	// lock: &(&proc_inum_lock)->rlock
	__raw_spin_unlock_irq(lock);
}
EXPORT_SYMBOL(_raw_spin_unlock_irq);
#endif

#ifndef CONFIG_INLINE_SPIN_UNLOCK_BH
void __lockfunc _raw_spin_unlock_bh(raw_spinlock_t *lock)
{
	__raw_spin_unlock_bh(lock);
}
EXPORT_SYMBOL(_raw_spin_unlock_bh);
#endif

#ifndef CONFIG_INLINE_READ_TRYLOCK
int __lockfunc _raw_read_trylock(rwlock_t *lock)
{
	return __raw_read_trylock(lock);
}
EXPORT_SYMBOL(_raw_read_trylock);
#endif

#ifndef CONFIG_INLINE_READ_LOCK // CONFIG_INLINE_READ_LOCK=n
// ARM10C 20160326
// &file_systems_lock
void __lockfunc _raw_read_lock(rwlock_t *lock)
{
	// lock: &file_systems_lock
	__raw_read_lock(lock);

	// __raw_read_lock 에서 한일:
	// &(&(&file_systems_lock)->raw_lock)->lock 의 값을 미리 cache에 가져옴
	// &(&(&file_systems_lock)->raw_lock)->lock 의 값을 1을 더해줌
	// 공유자원을 다른 cpu core가 사용할수 있게 해주는 옵션
}
EXPORT_SYMBOL(_raw_read_lock);
#endif

#ifndef CONFIG_INLINE_READ_LOCK_IRQSAVE
unsigned long __lockfunc _raw_read_lock_irqsave(rwlock_t *lock)
{
	return __raw_read_lock_irqsave(lock);
}
EXPORT_SYMBOL(_raw_read_lock_irqsave);
#endif

#ifndef CONFIG_INLINE_READ_LOCK_IRQ
void __lockfunc _raw_read_lock_irq(rwlock_t *lock)
{
	__raw_read_lock_irq(lock);
}
EXPORT_SYMBOL(_raw_read_lock_irq);
#endif

#ifndef CONFIG_INLINE_READ_LOCK_BH
void __lockfunc _raw_read_lock_bh(rwlock_t *lock)
{
	__raw_read_lock_bh(lock);
}
EXPORT_SYMBOL(_raw_read_lock_bh);
#endif

#ifndef CONFIG_INLINE_READ_UNLOCK
// ARM10C 20160402
// &file_systems_lock
void __lockfunc _raw_read_unlock(rwlock_t *lock)
{
	// lock: &file_systems_lock
	__raw_read_unlock(lock);

	// __raw_read_unlock 에서 한일:
	// &(&(&file_systems_lock)->raw_lock)->lock 의 값을 미리 cache에 가져옴
	// &(&(&file_systems_lock)->raw_lock)->lock 의 값을 1 만큼 값을 감소 시킴
	// Inner Shareable domain에 포함되어 있는 core 들의 instruction이 완료 될때 까지 기다리 겠다는 뜻.
	// 다중 프로세서 시스템 내의 모든 코어에 신호를 보낼 이벤트를 발생시킴
	// current_thread_info()->preempt_count: 0x40000001
}
EXPORT_SYMBOL(_raw_read_unlock);
#endif

#ifndef CONFIG_INLINE_READ_UNLOCK_IRQRESTORE
void __lockfunc _raw_read_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
	__raw_read_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(_raw_read_unlock_irqrestore);
#endif

#ifndef CONFIG_INLINE_READ_UNLOCK_IRQ
void __lockfunc _raw_read_unlock_irq(rwlock_t *lock)
{
	__raw_read_unlock_irq(lock);
}
EXPORT_SYMBOL(_raw_read_unlock_irq);
#endif

#ifndef CONFIG_INLINE_READ_UNLOCK_BH
void __lockfunc _raw_read_unlock_bh(rwlock_t *lock)
{
	__raw_read_unlock_bh(lock);
}
EXPORT_SYMBOL(_raw_read_unlock_bh);
#endif

#ifndef CONFIG_INLINE_WRITE_TRYLOCK
int __lockfunc _raw_write_trylock(rwlock_t *lock)
{
	return __raw_write_trylock(lock);
}
EXPORT_SYMBOL(_raw_write_trylock);
#endif

#ifndef CONFIG_INLINE_WRITE_LOCK // CONFIG_INLINE_WRITE_LOCK=n
// ARM10C 20140125
// ARM10C 20151031
// &file_systems_lock
void __lockfunc _raw_write_lock(rwlock_t *lock)
{
	__raw_write_lock(lock);
}
EXPORT_SYMBOL(_raw_write_lock);
#endif

#ifndef CONFIG_INLINE_WRITE_LOCK_IRQSAVE
unsigned long __lockfunc _raw_write_lock_irqsave(rwlock_t *lock)
{
	return __raw_write_lock_irqsave(lock);
}
EXPORT_SYMBOL(_raw_write_lock_irqsave);
#endif

#ifndef CONFIG_INLINE_WRITE_LOCK_IRQ
// ARM10C 20161203
// &tasklist_lock
void __lockfunc _raw_write_lock_irq(rwlock_t *lock)
{
	// lock: &tasklist_lock
	__raw_write_lock_irq(lock);
}
EXPORT_SYMBOL(_raw_write_lock_irq);
#endif

#ifndef CONFIG_INLINE_WRITE_LOCK_BH
void __lockfunc _raw_write_lock_bh(rwlock_t *lock)
{
	__raw_write_lock_bh(lock);
}
EXPORT_SYMBOL(_raw_write_lock_bh);
#endif

#ifndef CONFIG_INLINE_WRITE_UNLOCK // CONFIG_INLINE_WRITE_UNLOCK=n
// ARM10C 20140125
void __lockfunc _raw_write_unlock(rwlock_t *lock)
{
	__raw_write_unlock(lock);
}
EXPORT_SYMBOL(_raw_write_unlock);
#endif

#ifndef CONFIG_INLINE_WRITE_UNLOCK_IRQRESTORE
void __lockfunc _raw_write_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
	__raw_write_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(_raw_write_unlock_irqrestore);
#endif

#ifndef CONFIG_INLINE_WRITE_UNLOCK_IRQ
// ARM10C 20161210
// &tasklist_lock
void __lockfunc _raw_write_unlock_irq(rwlock_t *lock)
{
	__raw_write_unlock_irq(lock);
}
EXPORT_SYMBOL(_raw_write_unlock_irq);
#endif

#ifndef CONFIG_INLINE_WRITE_UNLOCK_BH
void __lockfunc _raw_write_unlock_bh(rwlock_t *lock)
{
	__raw_write_unlock_bh(lock);
}
EXPORT_SYMBOL(_raw_write_unlock_bh);
#endif

#ifdef CONFIG_DEBUG_LOCK_ALLOC

void __lockfunc _raw_spin_lock_nested(raw_spinlock_t *lock, int subclass)
{
	preempt_disable();
	spin_acquire(&lock->dep_map, subclass, 0, _RET_IP_);
	LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
}
EXPORT_SYMBOL(_raw_spin_lock_nested);

unsigned long __lockfunc _raw_spin_lock_irqsave_nested(raw_spinlock_t *lock,
						   int subclass)
{
	unsigned long flags;

	local_irq_save(flags);
	preempt_disable();
	spin_acquire(&lock->dep_map, subclass, 0, _RET_IP_);
	LOCK_CONTENDED_FLAGS(lock, do_raw_spin_trylock, do_raw_spin_lock,
				do_raw_spin_lock_flags, &flags);
	return flags;
}
EXPORT_SYMBOL(_raw_spin_lock_irqsave_nested);

void __lockfunc _raw_spin_lock_nest_lock(raw_spinlock_t *lock,
				     struct lockdep_map *nest_lock)
{
	preempt_disable();
	spin_acquire_nest(&lock->dep_map, 0, 0, nest_lock, _RET_IP_);
	LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
}
EXPORT_SYMBOL(_raw_spin_lock_nest_lock);

#endif

notrace int in_lock_functions(unsigned long addr)
{
	/* Linker adds these: start and end of __lockfunc functions */
	extern char __lock_text_start[], __lock_text_end[];

	return addr >= (unsigned long)__lock_text_start
	&& addr < (unsigned long)__lock_text_end;
}
EXPORT_SYMBOL(in_lock_functions);
