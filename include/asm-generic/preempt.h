#ifndef __ASM_PREEMPT_H
#define __ASM_PREEMPT_H

#include <linux/thread_info.h>

// ARM10C 20140913
// PREEMPT_ENABLED: 0
#define PREEMPT_ENABLED	(0)

// ARM10C 20130824
// ARM10C 20140315
static __always_inline int preempt_count(void)
{
	return current_thread_info()->preempt_count;
}

// a10c 4361
static __always_inline int *preempt_count_ptr(void)
{
	return &current_thread_info()->preempt_count;
}

static __always_inline void preempt_count_set(int pc)
{
	*preempt_count_ptr() = pc;
}

/*
 * must be macros to avoid header recursion hell
 */
#define task_preempt_count(p) \
	(task_thread_info(p)->preempt_count & ~PREEMPT_NEED_RESCHED)

#define init_task_preempt_count(p) do { \
	task_thread_info(p)->preempt_count = PREEMPT_DISABLED; \
} while (0)

// ARM10C 20140913
// idle: &init_task, cpu: 0
// PREEMPT_ENABLED: 0
// task_thread_info(&init_task): ((struct thread_info *)(&init_task)->stack)
//
// #define init_idle_preempt_count(&init_task, cpu) do {
// 	((struct thread_info *)(&init_task)->stack)->preempt_count = 0;
// } while (0)
#define init_idle_preempt_count(p, cpu) do { \
	task_thread_info(p)->preempt_count = PREEMPT_ENABLED; \
} while (0)

static __always_inline void set_preempt_need_resched(void)
{
}

static __always_inline void clear_preempt_need_resched(void)
{
}

static __always_inline bool test_preempt_need_resched(void)
{
	return false;
}

/*
 * The various preempt_count add/sub methods
 */

// a10c 4361
static __always_inline void __preempt_count_add(int val)
{
	*preempt_count_ptr() += val;
}

static __always_inline void __preempt_count_sub(int val)
{
	*preempt_count_ptr() -= val;
}

static __always_inline bool __preempt_count_dec_and_test(void)
{
	/*
	 * Because of load-store architectures cannot do per-cpu atomic
	 * operations; we cannot use PREEMPT_NEED_RESCHED because it might get
	 * lost.
	 */
	return !--*preempt_count_ptr() && tif_need_resched();
}

/*
 * Returns true when we need to resched and can (barring IRQ state).
 */
static __always_inline bool should_resched(void)
{
	return unlikely(!preempt_count() && tif_need_resched());
}

#ifdef CONFIG_PREEMPT // CONFIG_PREEMPT=y
// ARM10C 20140614
extern asmlinkage void preempt_schedule(void);
#define __preempt_schedule() preempt_schedule()

#ifdef CONFIG_CONTEXT_TRACKING
extern asmlinkage void preempt_schedule_context(void);
#define __preempt_schedule_context() preempt_schedule_context()
#endif
#endif /* CONFIG_PREEMPT */

#endif /* __ASM_PREEMPT_H */
