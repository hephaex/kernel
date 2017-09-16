/*
 * linux/kernel/irq/manage.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006 Thomas Gleixner
 *
 * This file contains driver APIs to the irq subsystem.
 */

#define pr_fmt(fmt) "genirq: " fmt

#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/task_work.h>

#include "internals.h"

#ifdef CONFIG_IRQ_FORCED_THREADING // CONFIG_IRQ_FORCED_THREADING=y
// ARM10C 20150523
__read_mostly bool force_irqthreads;

static int __init setup_forced_irqthreads(char *arg)
{
	force_irqthreads = true;
	return 0;
}
early_param("threadirqs", setup_forced_irqthreads);
#endif

/**
 *	synchronize_irq - wait for pending IRQ handlers (on other CPUs)
 *	@irq: interrupt number to wait for
 *
 *	This function waits for any pending IRQ handlers for this interrupt
 *	to complete before returning. If you use this function while
 *	holding a resource the IRQ handler may need you will deadlock.
 *
 *	This function may be called - with care - from IRQ context.
 */
void synchronize_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	bool inprogress;

	if (!desc)
		return;

	do {
		unsigned long flags;

		/*
		 * Wait until we're out of the critical section.  This might
		 * give the wrong answer due to the lack of memory barriers.
		 */
		while (irqd_irq_inprogress(&desc->irq_data))
			cpu_relax();

		/* Ok, that indicated we're done: double-check carefully. */
		raw_spin_lock_irqsave(&desc->lock, flags);
		inprogress = irqd_irq_inprogress(&desc->irq_data);
		raw_spin_unlock_irqrestore(&desc->lock, flags);

		/* Oops, that failed? */
	} while (inprogress);

	/*
	 * We made sure that no hardirq handler is running. Now verify
	 * that no threaded handlers are active.
	 */
	wait_event(desc->wait_for_threads, !atomic_read(&desc->threads_active));
}
EXPORT_SYMBOL(synchronize_irq);

#ifdef CONFIG_SMP // CONFIG_SMP=y
// ARM10C 20141004
// ARM10C 20150516
// ARM10C 20150523
cpumask_var_t irq_default_affinity;

/**
 *	irq_can_set_affinity - Check if the affinity of a given irq can be set
 *	@irq:		Interrupt to check
 *
 */
// ARM10C 20150516
// irq: 152
// ARM10C 20150523
// irq: 347
int irq_can_set_affinity(unsigned int irq)
{
	// irq: 152, irq_to_desc(152): kmem_cache#28-oX (irq 152)
	// irq: 347, irq_to_desc(347): kmem_cache#28-oX (irq 347)
	struct irq_desc *desc = irq_to_desc(irq);
	// desc: kmem_cache#28-oX (irq 152)
	// desc: kmem_cache#28-oX (irq 347)

	// desc: kmem_cache#28-oX (irq 152)
	// &desc->irq_data: &(kmem_cache#28-oX (irq 152))->irq_data,
	// irqd_can_balance(&(kmem_cache#28-oX (irq 152))->irq_data): 0
	// desc->irq_data.chip: (kmem_cache#28-oX (irq 152))->irq_data.chip: &gic_chip,
	// desc->irq_data.chip->irq_set_affinity: (kmem_cache#28-oX (irq 152))->irq_data.chip->irq_set_affinity: gic_set_affinity
	// desc: kmem_cache#28-oX (irq 347)
	// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data,
	// irqd_can_balance(&(kmem_cache#28-oX (irq 347))->irq_data): 1
	// desc->irq_data.chip: (kmem_cache#28-oX (irq 347))->irq_data.chip: &combiner_chip,
	// desc->irq_data.chip->irq_set_affinity: (kmem_cache#28-oX (irq 347))->irq_data.chip->irq_set_affinity: combiner_set_affinity
	if (!desc || !irqd_can_balance(&desc->irq_data) ||
	    !desc->irq_data.chip || !desc->irq_data.chip->irq_set_affinity)
		return 0;
		// return 0

	return 1;
	// return 1
}

/**
 *	irq_set_thread_affinity - Notify irq threads to adjust affinity
 *	@desc:		irq descriptor which has affitnity changed
 *
 *	We just set IRQTF_AFFINITY and delegate the affinity setting
 *	to the interrupt thread itself. We can not call
 *	set_cpus_allowed_ptr() here as we hold desc->lock and this
 *	code can be called from hard interrupt context.
 */
// ARM10C 20150404
// desc: kmem_cache#28-oX (irq 152)
// ARM10C 20150523
// desc: kmem_cache#28-oX (irq 347)
void irq_set_thread_affinity(struct irq_desc *desc)
{
	// desc->action: (kmem_cache#28-oX (irq 152)->action: NULL
	// desc->action: (kmem_cache#28-oX (irq 347)->action: NULL
	struct irqaction *action = desc->action;
	// action: NULL
	// action: NULL

	// action: NULL
	// action: NULL
	while (action) {
		if (action->thread)
			set_bit(IRQTF_AFFINITY, &action->thread_flags);
		action = action->next;
	}
}

#ifdef CONFIG_GENERIC_PENDING_IRQ
static inline bool irq_can_move_pcntxt(struct irq_data *data)
{
	return irqd_can_move_in_process_context(data);
}
static inline bool irq_move_pending(struct irq_data *data)
{
	return irqd_is_setaffinity_pending(data);
}
static inline void
irq_copy_pending(struct irq_desc *desc, const struct cpumask *mask)
{
	cpumask_copy(desc->pending_mask, mask);
}
static inline void
irq_get_pending(struct cpumask *mask, struct irq_desc *desc)
{
	cpumask_copy(mask, desc->pending_mask);
}
#else
// ARM10C 20150328
static inline bool irq_can_move_pcntxt(struct irq_data *data) { return true; }
static inline bool irq_move_pending(struct irq_data *data) { return false; }
static inline void
irq_copy_pending(struct irq_desc *desc, const struct cpumask *mask) { }
static inline void
irq_get_pending(struct cpumask *mask, struct irq_desc *desc) { }
#endif

// ARM10C 20150328
// data: &(kmem_cache#28-oX (irq 152))->irq_data, mask: &cpu_bit_bitmap[1][0], false
// ARM10C 20150523
// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data, mask, false
int irq_do_set_affinity(struct irq_data *data, const struct cpumask *mask,
			bool force)
{
	// data: &(kmem_cache#28-oX (irq 152))->irq_data,
	// irq_data_to_desc(&(kmem_cache#28-oX (irq 152))->irq_data): kmem_cache#28-oX (irq 152)
	// data: &(kmem_cache#28-oX (irq 347))->irq_data,
	// irq_data_to_desc(&(kmem_cache#28-oX (irq 347))->irq_data): kmem_cache#28-oX (irq 347)
	struct irq_desc *desc = irq_data_to_desc(data);
	// desc: kmem_cache#28-oX (irq 152)
	// desc: kmem_cache#28-oX (irq 347)

	// data: &(kmem_cache#28-oX (irq 152))->irq_data,
	// irq_data_get_irq_chip(&(kmem_cache#28-oX (irq 152))->irq_data):
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->chip: &gic_chip
	// data: &(kmem_cache#28-oX (irq 347))->irq_data,
	// irq_data_get_irq_chip(&(kmem_cache#28-oX (irq 347))->irq_data):
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->chip: &combiner_chip
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	// chip: &gic_chip
	// chip: &combiner_chip

	int ret;

	// chip->irq_set_affinity: (&gic_chip)->irq_set_affinity: gic_set_affinity
	// data: &(kmem_cache#28-oX (irq 152))->irq_data, mask: &cpu_bit_bitmap[1][0],
	// gic_set_affinity(&(kmem_cache#28-oX (irq 152))->irq_data, &cpu_bit_bitmap[1][0], false): 0
	// chip->irq_set_affinity: (&combiner_chip)->irq_set_affinity: combiner_set_affinity
	// data: &(kmem_cache#28-oX (irq 347))->irq_data, mask: mask,
	// combiner_set_affinity(&(kmem_cache#28-oX (irq 347))->irq_data, mask, false): 0
	ret = chip->irq_set_affinity(data, mask, false);
	// ret: 0
	// ret: 0

	// gic_set_affinity에서 한일:
	// Interrupt pending register인 GICD_ITARGETSR38 값을 읽고
	// 그 값과 mask 값인 cpu_bit_bitmap[1][0] 을 or 연산한 값을 GICD_ITARGETSR38에
	// 다시 write함
	//
	// GICD_ITARGETSR38 값을 모르기 때문에 0x00000000 로
	// 읽히는 것으로 가정하고 GICD_ITARGETSR38에 0x00000001를 write 함
	// CPU interface 0에 interrupt가 발생을 나타냄

	// combiner_set_affinity에서 한일:
	// GICD_ITARGETSR46 값을 모르기 때문에 0x00000000 로
	// 읽히는 것으로 가정하고 GICD_ITARGETSR46에 0x1000000를 write 함
	// CPU interface 0에 interrupt가 발생을 나타냄

	// ret: 0, IRQ_SET_MASK_OK: 0
	// ret: 0, IRQ_SET_MASK_OK: 0
	switch (ret) {
	case IRQ_SET_MASK_OK:
		// data->affinity: (&(kmem_cache#28-oX (irq 152))->irq_data)->affinity, mask: &cpu_bit_bitmap[1][0]
		// data->affinity: (&(kmem_cache#28-oX (irq 347))->irq_data)->affinity, mask: mask
		cpumask_copy(data->affinity, mask);

		// cpumask_copy에서 한일:
		// (&(kmem_cache#28-oX (irq 152))->irq_data)->affinity->bits[0]: 1

		// cpumask_copy에서 한일:
		// (&(kmem_cache#28-oX (irq 347))->irq_data)->affinity->bits[0]: 1

	case IRQ_SET_MASK_OK_NOCOPY:
		// desc: kmem_cache#28-oX (irq 152)
		// desc: kmem_cache#28-oX (irq 347)
		irq_set_thread_affinity(desc);
		ret = 0;
		// ret: 0
	}

	// ret: 0
	// ret: 0
	return ret;
	// return 0
	// return 0
}

// ARM10C 20150328
// &(kmem_cache#28-oX (irq 152))->irq_data, mask: &cpu_bit_bitmap[1][0]
int __irq_set_affinity_locked(struct irq_data *data, const struct cpumask *mask)
{
	// data: &(kmem_cache#28-oX (irq 152))->irq_data
	// irq_data_get_irq_chip(&(kmem_cache#28-oX (irq 152))->irq_data):
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->chip: &gic_chip
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	// chip: &gic_chip

	// data: &(kmem_cache#28-oX (irq 152))->irq_data
	// irq_data_to_desc(&(kmem_cache#28-oX (irq 152))->irq_data): kmem_cache#28-oX (irq 152)
	struct irq_desc *desc = irq_data_to_desc(data);
	// desc: kmem_cache#28-oX (irq 152)

	int ret = 0;
	// ret: 0

	// chip: &gic_chip, chip->irq_set_affinity: (&gic_chip)->irq_set_affinity: gic_set_affinity
	if (!chip || !chip->irq_set_affinity)
		return -EINVAL;

	// data: &(kmem_cache#28-oX (irq 152))->irq_data
	// irq_can_move_pcntxt(&(kmem_cache#28-oX (irq 152))->irq_data): true
	if (irq_can_move_pcntxt(data)) {
		// data: &(kmem_cache#28-oX (irq 152))->irq_data, mask: &cpu_bit_bitmap[1][0]
		// irq_do_set_affinity(&(kmem_cache#28-oX (irq 152))->irq_data, &cpu_bit_bitmap[1][0]): 0
		ret = irq_do_set_affinity(data, mask, false);
		// ret: 0

		// irq_do_set_affinity에서 한일:
		// Interrupt pending register인 GICD_ITARGETSR38 값을 읽고
		// 그 값과 mask 값인 cpu_bit_bitmap[1][0] 을 or 연산한 값을 GICD_ITARGETSR38에
		// 다시 write함
		//
		// GICD_ITARGETSR38 값을 모르기 때문에 0x00000000 로
		// 읽히는 것으로 가정하고 GICD_ITARGETSR38에 0x00000001를 write 함
		// CPU interface 0에 interrupt가 발생을 나타냄
		//
		// (&(kmem_cache#28-oX (irq 152))->irq_data)->affinity->bits[0]: 1
	} else {
		irqd_set_move_pending(data);
		irq_copy_pending(desc, mask);
	}

	// data->affinity_notify: (&(kmem_cache#28-oX (irq 152))->irq_data)->affinity_notify: NULL
	if (desc->affinity_notify) {
		kref_get(&desc->affinity_notify->kref);
		schedule_work(&desc->affinity_notify->work);
	}

	// data: &(kmem_cache#28-oX (irq 152))->irq_data, IRQD_AFFINITY_SET: 0x1000
	irqd_set(data, IRQD_AFFINITY_SET);

	// irqd_set에서 한일:
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11000

	// ret: 0
	return ret;
	// return 0
}

/**
 *	irq_set_affinity - Set the irq affinity of a given irq
 *	@irq:		Interrupt to set affinity
 *	@mask:		cpumask
 *
 */
// ARM10C 20150328
// mct_irqs[4]: 152, cpumask_of(0): &cpu_bit_bitmap[1][0]
int irq_set_affinity(unsigned int irq, const struct cpumask *mask)
{
	// irq: 152, irq_to_desc(152): kmem_cache#28-oX (irq 152)
	struct irq_desc *desc = irq_to_desc(irq);
	// desc: kmem_cache#28-oX (irq 152)

	unsigned long flags;
	int ret;

	// desc: kmem_cache#28-oX (irq 152)
	if (!desc)
		return -EINVAL;

	// &desc->lock: (kmem_cache#28-oX (irq 152))->lock
	raw_spin_lock_irqsave(&desc->lock, flags);

	// raw_spin_lock_irqsave에서 한일:
	// (kmem_cache#28-oX (irq 152))->lock을 사용하여 spin lock을 수행하고 cpsr을 flags에 저장

	// desc: kmem_cache#28-oX (irq 152)
	// irq_desc_get_irq_data(kmem_cache#28-oX (irq 152)): &(kmem_cache#28-oX (irq 152))->irq_data,
	// mask: &cpu_bit_bitmap[1][0]
	// __irq_set_affinity_locked(&(kmem_cache#28-oX (irq 152))->irq_data, &cpu_bit_bitmap[1][0]):  0
	ret =  __irq_set_affinity_locked(irq_desc_get_irq_data(desc), mask);
	// ret: 0

	// __irq_set_affinity_locked 에서 한일:
	//
	// Interrupt pending register인 GICD_ITARGETSR38 값을 읽고
	// 그 값과 mask 값인 cpu_bit_bitmap[1][0] 을 or 연산한 값을 GICD_ITARGETSR38에
	// 다시 write함
	//
	// GICD_ITARGETSR38 값을 모르기 때문에 0x00000000 로
	// 읽히는 것으로 가정하고 GICD_ITARGETSR38에 0x00000001를 write 함
	// CPU interface 0에 interrupt가 발생을 나타냄
	//
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->affinity->bits[0]: 1
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11000

	// &desc->lock: (kmem_cache#28-oX (irq 152))->lock
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	// raw_spin_unlock_irqrestore에서 한일:
	// (kmem_cache#28-oX (irq 152))->lock을 사용하여 spin unlock을 수행하고 flags에 저장된 cpsr을 복원

	// ret: 0
	return ret;
	// return 0
}

int irq_set_affinity_hint(unsigned int irq, const struct cpumask *m)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

	if (!desc)
		return -EINVAL;
	desc->affinity_hint = m;
	irq_put_desc_unlock(desc, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(irq_set_affinity_hint);

static void irq_affinity_notify(struct work_struct *work)
{
	struct irq_affinity_notify *notify =
		container_of(work, struct irq_affinity_notify, work);
	struct irq_desc *desc = irq_to_desc(notify->irq);
	cpumask_var_t cpumask;
	unsigned long flags;

	if (!desc || !alloc_cpumask_var(&cpumask, GFP_KERNEL))
		goto out;

	raw_spin_lock_irqsave(&desc->lock, flags);
	if (irq_move_pending(&desc->irq_data))
		irq_get_pending(cpumask, desc);
	else
		cpumask_copy(cpumask, desc->irq_data.affinity);
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	notify->notify(notify, cpumask);

	free_cpumask_var(cpumask);
out:
	kref_put(&notify->kref, notify->release);
}

/**
 *	irq_set_affinity_notifier - control notification of IRQ affinity changes
 *	@irq:		Interrupt for which to enable/disable notification
 *	@notify:	Context for notification, or %NULL to disable
 *			notification.  Function pointers must be initialised;
 *			the other fields will be initialised by this function.
 *
 *	Must be called in process context.  Notification may only be enabled
 *	after the IRQ is allocated and must be disabled before the IRQ is
 *	freed using free_irq().
 */
int
irq_set_affinity_notifier(unsigned int irq, struct irq_affinity_notify *notify)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irq_affinity_notify *old_notify;
	unsigned long flags;

	/* The release function is promised process context */
	might_sleep();

	if (!desc)
		return -EINVAL;

	/* Complete initialisation of *notify */
	if (notify) {
		notify->irq = irq;
		kref_init(&notify->kref);
		INIT_WORK(&notify->work, irq_affinity_notify);
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	old_notify = desc->affinity_notify;
	desc->affinity_notify = notify;
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	if (old_notify)
		kref_put(&old_notify->kref, old_notify->release);

	return 0;
}
EXPORT_SYMBOL_GPL(irq_set_affinity_notifier);

#ifndef CONFIG_AUTO_IRQ_AFFINITY // CONFIG_AUTO_IRQ_AFFINITY=n
/*
 * Generic version of the affinity autoselector.
 */
// ARM10C 20150516
// irq: 152, desc: kmem_cache#28-oX (irq 152), mask
// ARM10C 20150523
// irq: 347, desc: kmem_cache#28-oX (irq 347), mask
static int
setup_affinity(unsigned int irq, struct irq_desc *desc, struct cpumask *mask)
{
	struct cpumask *set = irq_default_affinity;
	// set: irq_default_affinity
	// set: irq_default_affinity

	// desc->irq_data.node: (kmem_cache#28-oX (irq 152))->irq_data.node: 0
	// desc->irq_data.node: (kmem_cache#28-oX (irq 347))->irq_data.node: 0
	int node = desc->irq_data.node;
	// node: 0
	// node: 0

	/* Excludes PER_CPU and NO_BALANCE interrupts */
	// irq: 152, irq_can_set_affinity(152): 0
	// irq: 347, irq_can_set_affinity(347): 1
	if (!irq_can_set_affinity(irq))
		return 0;
		// return 0

	/*
	 * Preserve an userspace affinity setup, but make sure that
	 * one of the targets is online.
	 */
	// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data, IRQD_AFFINITY_SET: 0x1000,
	// irqd_has_set(&(kmem_cache#28-oX (irq 347))->irq_data, 0x1000): 0
	if (irqd_has_set(&desc->irq_data, IRQD_AFFINITY_SET)) {
		if (cpumask_intersects(desc->irq_data.affinity,
				       cpu_online_mask))
			set = desc->irq_data.affinity;
		else
			irqd_clear(&desc->irq_data, IRQD_AFFINITY_SET);
	}

	// cpu_online_mask: cpu_online_bits, set: irq_default_affinity
	// cpumask_and(mask, cpu_online_bits, irq_default_affinity): 1
	cpumask_and(mask, cpu_online_mask, set);

	// cpumask_and에서 한일:
	// (mask)->bits[0]: 1

	// node: 0, NUMA_NO_NODE: -1
	if (node != NUMA_NO_NODE) {
		// node: 0, cpumask_of_node(0): ((void)0, cpu_online_mask)
		const struct cpumask *nodemask = cpumask_of_node(node);
		// nodemask: 0

		/* make sure at least one of the cpus in nodemask is online */
		// nodemask: 0, cpumask_intersects(mask, 0): 0
		if (cpumask_intersects(mask, nodemask))
			cpumask_and(mask, mask, nodemask);
	}

	// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data
	// irq_do_set_affinity(&(kmem_cache#28-oX (irq 347))->irq_data, mask, false): 0
	irq_do_set_affinity(&desc->irq_data, mask, false);

	// irq_do_set_affinity에서 한일:
	// GICD_ITARGETSR46 값을 모르기 때문에 0x00000000 로
	// 읽히는 것으로 가정하고 GICD_ITARGETSR46에 0x1000000를 write 함
	// CPU interface 0에 interrupt가 발생을 나타냄
	//
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->affinity->bits[0]: 1

	return 0;
	// return 0
}
#else
static inline int
setup_affinity(unsigned int irq, struct irq_desc *d, struct cpumask *mask)
{
	return irq_select_affinity(irq);
}
#endif

/*
 * Called when affinity is set via /proc/irq
 */
int irq_select_affinity_usr(unsigned int irq, struct cpumask *mask)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&desc->lock, flags);
	ret = setup_affinity(irq, desc, mask);
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return ret;
}

#else
static inline int
setup_affinity(unsigned int irq, struct irq_desc *desc, struct cpumask *mask)
{
	return 0;
}
#endif

void __disable_irq(struct irq_desc *desc, unsigned int irq, bool suspend)
{
	if (suspend) {
		if (!desc->action || (desc->action->flags & IRQF_NO_SUSPEND))
			return;
		desc->istate |= IRQS_SUSPENDED;
	}

	if (!desc->depth++)
		irq_disable(desc);
}

static int __disable_irq_nosync(unsigned int irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

	if (!desc)
		return -EINVAL;
	__disable_irq(desc, irq, false);
	irq_put_desc_busunlock(desc, flags);
	return 0;
}

/**
 *	disable_irq_nosync - disable an irq without waiting
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Disables and Enables are
 *	nested.
 *	Unlike disable_irq(), this function does not ensure existing
 *	instances of the IRQ handler have completed before returning.
 *
 *	This function may be called from IRQ context.
 */
void disable_irq_nosync(unsigned int irq)
{
	__disable_irq_nosync(irq);
}
EXPORT_SYMBOL(disable_irq_nosync);

/**
 *	disable_irq - disable an irq and wait for completion
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Enables and Disables are
 *	nested.
 *	This function waits for any pending IRQ handlers for this interrupt
 *	to complete before returning. If you use this function while
 *	holding a resource the IRQ handler may need you will deadlock.
 *
 *	This function may be called - with care - from IRQ context.
 */
void disable_irq(unsigned int irq)
{
	if (!__disable_irq_nosync(irq))
		synchronize_irq(irq);
}
EXPORT_SYMBOL(disable_irq);

void __enable_irq(struct irq_desc *desc, unsigned int irq, bool resume)
{
	if (resume) {
		if (!(desc->istate & IRQS_SUSPENDED)) {
			if (!desc->action)
				return;
			if (!(desc->action->flags & IRQF_FORCE_RESUME))
				return;
			/* Pretend that it got disabled ! */
			desc->depth++;
		}
		desc->istate &= ~IRQS_SUSPENDED;
	}

	switch (desc->depth) {
	case 0:
 err_out:
		WARN(1, KERN_WARNING "Unbalanced enable for IRQ %d\n", irq);
		break;
	case 1: {
		if (desc->istate & IRQS_SUSPENDED)
			goto err_out;
		/* Prevent probing on this irq: */
		irq_settings_set_noprobe(desc);
		irq_enable(desc);
		check_irq_resend(desc, irq);
		/* fall-through */
	}
	default:
		desc->depth--;
	}
}

/**
 *	enable_irq - enable handling of an irq
 *	@irq: Interrupt to enable
 *
 *	Undoes the effect of one call to disable_irq().  If this
 *	matches the last disable, processing of interrupts on this
 *	IRQ line is re-enabled.
 *
 *	This function may be called from IRQ context only when
 *	desc->irq_data.chip->bus_lock and desc->chip->bus_sync_unlock are NULL !
 */
void enable_irq(unsigned int irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

	if (!desc)
		return;
	if (WARN(!desc->irq_data.chip,
		 KERN_ERR "enable_irq before setup/request_irq: irq %u\n", irq))
		goto out;

	__enable_irq(desc, irq, false);
out:
	irq_put_desc_busunlock(desc, flags);
}
EXPORT_SYMBOL(enable_irq);

static int set_irq_wake_real(unsigned int irq, unsigned int on)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int ret = -ENXIO;

	if (irq_desc_get_chip(desc)->flags &  IRQCHIP_SKIP_SET_WAKE)
		return 0;

	if (desc->irq_data.chip->irq_set_wake)
		ret = desc->irq_data.chip->irq_set_wake(&desc->irq_data, on);

	return ret;
}

/**
 *	irq_set_irq_wake - control irq power management wakeup
 *	@irq:	interrupt to control
 *	@on:	enable/disable power management wakeup
 *
 *	Enable/disable power management wakeup mode, which is
 *	disabled by default.  Enables and disables must match,
 *	just as they match for non-wakeup mode support.
 *
 *	Wakeup mode lets this IRQ wake the system from sleep
 *	states like "suspend to RAM".
 */
int irq_set_irq_wake(unsigned int irq, unsigned int on)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);
	int ret = 0;

	if (!desc)
		return -EINVAL;

	/* wakeup-capable irqs can be shared between drivers that
	 * don't need to have the same sleep mode behaviors.
	 */
	if (on) {
		if (desc->wake_depth++ == 0) {
			ret = set_irq_wake_real(irq, on);
			if (ret)
				desc->wake_depth = 0;
			else
				irqd_set(&desc->irq_data, IRQD_WAKEUP_STATE);
		}
	} else {
		if (desc->wake_depth == 0) {
			WARN(1, "Unbalanced IRQ %d wake disable\n", irq);
		} else if (--desc->wake_depth == 0) {
			ret = set_irq_wake_real(irq, on);
			if (ret)
				desc->wake_depth = 1;
			else
				irqd_clear(&desc->irq_data, IRQD_WAKEUP_STATE);
		}
	}
	irq_put_desc_busunlock(desc, flags);
	return ret;
}
EXPORT_SYMBOL(irq_set_irq_wake);

/*
 * Internal function that tells the architecture code whether a
 * particular irq has been exclusively allocated or is available
 * for driver use.
 */
int can_request_irq(unsigned int irq, unsigned long irqflags)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, 0);
	int canrequest = 0;

	if (!desc)
		return 0;

	if (irq_settings_can_request(desc)) {
		if (!desc->action ||
		    irqflags & desc->action->flags & IRQF_SHARED)
			canrequest = 1;
	}
	irq_put_desc_unlock(desc, flags);
	return canrequest;
}

int __irq_set_trigger(struct irq_desc *desc, unsigned int irq,
		      unsigned long flags)
{
	struct irq_chip *chip = desc->irq_data.chip;
	int ret, unmask = 0;

	if (!chip || !chip->irq_set_type) {
		/*
		 * IRQF_TRIGGER_* but the PIC does not support multiple
		 * flow-types?
		 */
		pr_debug("No set_type function for IRQ %d (%s)\n", irq,
			 chip ? (chip->name ? : "unknown") : "unknown");
		return 0;
	}

	flags &= IRQ_TYPE_SENSE_MASK;

	if (chip->flags & IRQCHIP_SET_TYPE_MASKED) {
		if (!irqd_irq_masked(&desc->irq_data))
			mask_irq(desc);
		if (!irqd_irq_disabled(&desc->irq_data))
			unmask = 1;
	}

	/* caller masked out all except trigger mode flags */
	ret = chip->irq_set_type(&desc->irq_data, flags);

	switch (ret) {
	case IRQ_SET_MASK_OK:
		irqd_clear(&desc->irq_data, IRQD_TRIGGER_MASK);
		irqd_set(&desc->irq_data, flags);

	case IRQ_SET_MASK_OK_NOCOPY:
		flags = irqd_get_trigger_type(&desc->irq_data);
		irq_settings_set_trigger_mask(desc, flags);
		irqd_clear(&desc->irq_data, IRQD_LEVEL);
		irq_settings_clr_level(desc);
		if (flags & IRQ_TYPE_LEVEL_MASK) {
			irq_settings_set_level(desc);
			irqd_set(&desc->irq_data, IRQD_LEVEL);
		}

		ret = 0;
		break;
	default:
		pr_err("Setting trigger mode %lu for irq %u failed (%pF)\n",
		       flags, irq, chip->irq_set_type);
	}
	if (unmask)
		unmask_irq(desc);
	return ret;
}

#ifdef CONFIG_HARDIRQS_SW_RESEND
int irq_set_parent(int irq, int parent_irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, 0);

	if (!desc)
		return -EINVAL;

	desc->parent_irq = parent_irq;

	irq_put_desc_unlock(desc, flags);
	return 0;
}
#endif

/*
 * Default primary interrupt handler for threaded interrupts. Is
 * assigned as primary handler when request_threaded_irq is called
 * with handler == NULL. Useful for oneshot interrupts.
 */
static irqreturn_t irq_default_primary_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

/*
 * Primary handler for nested threaded interrupts. Should never be
 * called.
 */
static irqreturn_t irq_nested_primary_handler(int irq, void *dev_id)
{
	WARN(1, "Primary handler called for nested irq %d\n", irq);
	return IRQ_NONE;
}

static int irq_wait_for_interrupt(struct irqaction *action)
{
	set_current_state(TASK_INTERRUPTIBLE);

	while (!kthread_should_stop()) {

		if (test_and_clear_bit(IRQTF_RUNTHREAD,
				       &action->thread_flags)) {
			__set_current_state(TASK_RUNNING);
			return 0;
		}
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
	return -1;
}

/*
 * Oneshot interrupts keep the irq line masked until the threaded
 * handler finished. unmask if the interrupt has not been disabled and
 * is marked MASKED.
 */
static void irq_finalize_oneshot(struct irq_desc *desc,
				 struct irqaction *action)
{
	if (!(desc->istate & IRQS_ONESHOT))
		return;
again:
	chip_bus_lock(desc);
	raw_spin_lock_irq(&desc->lock);

	/*
	 * Implausible though it may be we need to protect us against
	 * the following scenario:
	 *
	 * The thread is faster done than the hard interrupt handler
	 * on the other CPU. If we unmask the irq line then the
	 * interrupt can come in again and masks the line, leaves due
	 * to IRQS_INPROGRESS and the irq line is masked forever.
	 *
	 * This also serializes the state of shared oneshot handlers
	 * versus "desc->threads_onehsot |= action->thread_mask;" in
	 * irq_wake_thread(). See the comment there which explains the
	 * serialization.
	 */
	if (unlikely(irqd_irq_inprogress(&desc->irq_data))) {
		raw_spin_unlock_irq(&desc->lock);
		chip_bus_sync_unlock(desc);
		cpu_relax();
		goto again;
	}

	/*
	 * Now check again, whether the thread should run. Otherwise
	 * we would clear the threads_oneshot bit of this thread which
	 * was just set.
	 */
	if (test_bit(IRQTF_RUNTHREAD, &action->thread_flags))
		goto out_unlock;

	desc->threads_oneshot &= ~action->thread_mask;

	if (!desc->threads_oneshot && !irqd_irq_disabled(&desc->irq_data) &&
	    irqd_irq_masked(&desc->irq_data))
		unmask_irq(desc);

out_unlock:
	raw_spin_unlock_irq(&desc->lock);
	chip_bus_sync_unlock(desc);
}

#ifdef CONFIG_SMP
/*
 * Check whether we need to chasnge the affinity of the interrupt thread.
 */
static void
irq_thread_check_affinity(struct irq_desc *desc, struct irqaction *action)
{
	cpumask_var_t mask;
	bool valid = true;

	if (!test_and_clear_bit(IRQTF_AFFINITY, &action->thread_flags))
		return;

	/*
	 * In case we are out of memory we set IRQTF_AFFINITY again and
	 * try again next time
	 */
	if (!alloc_cpumask_var(&mask, GFP_KERNEL)) {
		set_bit(IRQTF_AFFINITY, &action->thread_flags);
		return;
	}

	raw_spin_lock_irq(&desc->lock);
	/*
	 * This code is triggered unconditionally. Check the affinity
	 * mask pointer. For CPU_MASK_OFFSTACK=n this is optimized out.
	 */
	if (desc->irq_data.affinity)
		cpumask_copy(mask, desc->irq_data.affinity);
	else
		valid = false;
	raw_spin_unlock_irq(&desc->lock);

	if (valid)
		set_cpus_allowed_ptr(current, mask);
	free_cpumask_var(mask);
}
#else
static inline void
irq_thread_check_affinity(struct irq_desc *desc, struct irqaction *action) { }
#endif

/*
 * Interrupts which are not explicitely requested as threaded
 * interrupts rely on the implicit bh/preempt disable of the hard irq
 * context. So we need to disable bh here to avoid deadlocks and other
 * side effects.
 */
static irqreturn_t
irq_forced_thread_fn(struct irq_desc *desc, struct irqaction *action)
{
	irqreturn_t ret;

	local_bh_disable();
	ret = action->thread_fn(action->irq, action->dev_id);
	irq_finalize_oneshot(desc, action);
	local_bh_enable();
	return ret;
}

/*
 * Interrupts explicitly requested as threaded interrupts want to be
 * preemtible - many of them need to sleep and wait for slow busses to
 * complete.
 */
static irqreturn_t irq_thread_fn(struct irq_desc *desc,
		struct irqaction *action)
{
	irqreturn_t ret;

	ret = action->thread_fn(action->irq, action->dev_id);
	irq_finalize_oneshot(desc, action);
	return ret;
}

static void wake_threads_waitq(struct irq_desc *desc)
{
	if (atomic_dec_and_test(&desc->threads_active))
		wake_up(&desc->wait_for_threads);
}

static void irq_thread_dtor(struct callback_head *unused)
{
	struct task_struct *tsk = current;
	struct irq_desc *desc;
	struct irqaction *action;

	if (WARN_ON_ONCE(!(current->flags & PF_EXITING)))
		return;

	action = kthread_data(tsk);

	pr_err("exiting task \"%s\" (%d) is an active IRQ thread (irq %d)\n",
	       tsk->comm, tsk->pid, action->irq);


	desc = irq_to_desc(action->irq);
	/*
	 * If IRQTF_RUNTHREAD is set, we need to decrement
	 * desc->threads_active and wake possible waiters.
	 */
	if (test_and_clear_bit(IRQTF_RUNTHREAD, &action->thread_flags))
		wake_threads_waitq(desc);

	/* Prevent a stale desc->threads_oneshot */
	irq_finalize_oneshot(desc, action);
}

/*
 * Interrupt handler thread
 */
static int irq_thread(void *data)
{
	struct callback_head on_exit_work;
	struct irqaction *action = data;
	struct irq_desc *desc = irq_to_desc(action->irq);
	irqreturn_t (*handler_fn)(struct irq_desc *desc,
			struct irqaction *action);

	if (force_irqthreads && test_bit(IRQTF_FORCED_THREAD,
					&action->thread_flags))
		handler_fn = irq_forced_thread_fn;
	else
		handler_fn = irq_thread_fn;

	init_task_work(&on_exit_work, irq_thread_dtor);
	task_work_add(current, &on_exit_work, false);

	irq_thread_check_affinity(desc, action);

	while (!irq_wait_for_interrupt(action)) {
		irqreturn_t action_ret;

		irq_thread_check_affinity(desc, action);

		action_ret = handler_fn(desc, action);
		if (!noirqdebug)
			note_interrupt(action->irq, desc, action_ret);

		wake_threads_waitq(desc);
	}

	/*
	 * This is the regular exit path. __free_irq() is stopping the
	 * thread via kthread_stop() after calling
	 * synchronize_irq(). So neither IRQTF_RUNTHREAD nor the
	 * oneshot mask bit can be set. We cannot verify that as we
	 * cannot touch the oneshot mask at this point anymore as
	 * __setup_irq() might have given out currents thread_mask
	 * again.
	 */
	task_work_cancel(current, irq_thread_dtor);
	return 0;
}

// ARM10C 20150523
// new: &mct_comp_event_irq
static void irq_setup_forced_threading(struct irqaction *new)
{
	// force_irqthreads: 0
	if (!force_irqthreads)
		return;
		// return 수행

	if (new->flags & (IRQF_NO_THREAD | IRQF_PERCPU | IRQF_ONESHOT))
		return;

	new->flags |= IRQF_ONESHOT;

	if (!new->thread_fn) {
		set_bit(IRQTF_FORCED_THREAD, &new->thread_flags);
		new->thread_fn = new->handler;
		new->handler = irq_default_primary_handler;
	}
}

/*
 * Internal function to register an irqaction - typically used to
 * allocate special interrupts that are part of the architecture.
 */
// ARM10C 20150509
// irq: 152, desc: kmem_cache#28-oX (irq 152), action: kmem_cache#30-oX
// ARM10C 20150523
// irq: 347, desc: kmem_cache#28-oX (irq 347), act: &mct_comp_event_irq
static int
__setup_irq(unsigned int irq, struct irq_desc *desc, struct irqaction *new)
{
	struct irqaction *old, **old_ptr;
	unsigned long flags, thread_mask = 0;
	// thread_mask: 0
	// thread_mask: 0

	int ret, nested, shared = 0;
	// shared: 0
	// shared: 0

	cpumask_var_t mask;

	// desc: kmem_cache#28-oX (irq 152)
	// desc: kmem_cache#28-oX (irq 347)
	if (!desc)
		return -EINVAL;

	// desc->irq_data.chip: (kmem_cache#28-oX (irq 152))->irq_data.chip: &gic_chip
	// desc->irq_data.chip: (kmem_cache#28-oX (irq 347))->irq_data.chip: &combiner_chip
	if (desc->irq_data.chip == &no_irq_chip)
		return -ENOSYS;

	// desc->owner: (kmem_cache#28-oX (irq 152))->owner: NULL,
	// try_module_get((kmem_cache#28-oX (irq 152))->owner): true
	// desc->owner: (kmem_cache#28-oX (irq 347))->owner: NULL,
	// try_module_get((kmem_cache#28-oX (irq 347))->owner): true
	if (!try_module_get(desc->owner))
		return -ENODEV;

	/*
	 * Check whether the interrupt nests into another interrupt
	 * thread.
	 */
	// desc: kmem_cache#28-oX (irq 152),
	// irq_settings_is_nested_thread(kmem_cache#28-oX (irq 152)): 0
	// desc: kmem_cache#28-oX (irq 347),
	// irq_settings_is_nested_thread(kmem_cache#28-oX (irq 347)): 0
	nested = irq_settings_is_nested_thread(desc);
	// nested: 0
	// nested: 0

	// nested: 0
	// nested: 0
	if (nested) {
		if (!new->thread_fn) {
			ret = -EINVAL;
			goto out_mput;
		}
		/*
		 * Replace the primary handler which was provided from
		 * the driver for non nested interrupt handling by the
		 * dummy function which warns when called.
		 */
		new->handler = irq_nested_primary_handler;
	} else {
		// desc: kmem_cache#28-oX (irq 152),
		// irq_settings_can_thread(kmem_cache#28-oX (irq 152)): 0
		// desc: kmem_cache#28-oX (irq 347),
		// irq_settings_can_thread(kmem_cache#28-oX (irq 347)): 1
		if (irq_settings_can_thread(desc))
			// new: &mct_comp_event_irq
			irq_setup_forced_threading(new);
	}

	/*
	 * Create a handler thread when a thread function is supplied
	 * and the interrupt does not nest into another interrupt
	 * thread.
	 */
	// new->thread_fn: (kmem_cache#30-oX)->thread_fn: NULL, nested: 0
	// new->thread_fn: (&mct_comp_event_irq)->thread_fn: NULL, nested: 0
	if (new->thread_fn && !nested) {
		struct task_struct *t;
		static const struct sched_param param = {
			.sched_priority = MAX_USER_RT_PRIO/2,
		};

		t = kthread_create(irq_thread, new, "irq/%d-%s", irq,
				   new->name);
		if (IS_ERR(t)) {
			ret = PTR_ERR(t);
			goto out_mput;
		}

		sched_setscheduler_nocheck(t, SCHED_FIFO, &param);

		/*
		 * We keep the reference to the task struct even if
		 * the thread dies to avoid that the interrupt code
		 * references an already freed task_struct.
		 */
		get_task_struct(t);
		new->thread = t;
		/*
		 * Tell the thread to set its affinity. This is
		 * important for shared interrupt handlers as we do
		 * not invoke setup_affinity() for the secondary
		 * handlers as everything is already set up. Even for
		 * interrupts marked with IRQF_NO_BALANCE this is
		 * correct as we want the thread to move to the cpu(s)
		 * on which the requesting code placed the interrupt.
		 */
		set_bit(IRQTF_AFFINITY, &new->thread_flags);
	}

	// GFP_KERNEL: 0xD0, alloc_cpumask_var(&mask, 0xD0): true
	// GFP_KERNEL: 0xD0, alloc_cpumask_var(&mask, 0xD0): true
	if (!alloc_cpumask_var(&mask, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out_thread;
	}

	/*
	 * Drivers are often written to work w/o knowledge about the
	 * underlying irq chip implementation, so a request for a
	 * threaded irq without a primary hard irq context handler
	 * requires the ONESHOT flag to be set. Some irq chips like
	 * MSI based interrupts are per se one shot safe. Check the
	 * chip flags, so we can avoid the unmask dance at the end of
	 * the threaded handler for those.
	 */
	// desc->irq_data.chip->flags: (kmem_cache#28-oX (irq 152))->irq_data.chip->flags: 0,
	// IRQCHIP_ONESHOT_SAFE: 0x20
	// desc->irq_data.chip->flags: (kmem_cache#28-oX (irq 347))->irq_data.chip->flags: 0,
	// IRQCHIP_ONESHOT_SAFE: 0x20
	if (desc->irq_data.chip->flags & IRQCHIP_ONESHOT_SAFE)
		new->flags &= ~IRQF_ONESHOT;

	/*
	 * The following block of code has to be executed atomically
	 */
	// &desc->lock: (kmem_cache#28-oX (irq 152))->lock
	// &desc->lock: (kmem_cache#28-oX (irq 347))->lock
	raw_spin_lock_irqsave(&desc->lock, flags);

	// raw_spin_lock_irqsave에서 한일:
	// (kmem_cache#28-oX (irq 152))->lock을 사용하여 spin lock을 수행하고 cpsr을 flags에 저장

	// raw_spin_lock_irqsave에서 한일:
	// (kmem_cache#28-oX (irq 347))->lock을 사용하여 spin lock을 수행하고 cpsr을 flags에 저장

	// &desc->action: &(kmem_cache#28-oX (irq 152))->action
	// &desc->action: &(kmem_cache#28-oX (irq 347))->action
	old_ptr = &desc->action;
	// old_ptr: &(kmem_cache#28-oX (irq 152))->action
	// old_ptr: &(kmem_cache#28-oX (irq 347))->action

	// *old_ptr: *(&(kmem_cache#28-oX (irq 152))->action): NULL
	// *old_ptr: *(&(kmem_cache#28-oX (irq 347))->action): NULL
	old = *old_ptr;
	// old: NULL
	// old: NULL

	// old: NULL
	// old: NULL
	if (old) {
		/*
		 * Can't share interrupts unless both agree to and are
		 * the same type (level, edge, polarity). So both flag
		 * fields must have IRQF_SHARED set and the bits which
		 * set the trigger type must match. Also all must
		 * agree on ONESHOT.
		 */
		if (!((old->flags & new->flags) & IRQF_SHARED) ||
		    ((old->flags ^ new->flags) & IRQF_TRIGGER_MASK) ||
		    ((old->flags ^ new->flags) & IRQF_ONESHOT))
			goto mismatch;

		/* All handlers must agree on per-cpuness */
		if ((old->flags & IRQF_PERCPU) !=
		    (new->flags & IRQF_PERCPU))
			goto mismatch;

		/* add new interrupt at end of irq queue */
		do {
			/*
			 * Or all existing action->thread_mask bits,
			 * so we can find the next zero bit for this
			 * new action.
			 */
			thread_mask |= old->thread_mask;
			old_ptr = &old->next;
			old = *old_ptr;
		} while (old);
		shared = 1;
	}

	/*
	 * Setup the thread mask for this irqaction for ONESHOT. For
	 * !ONESHOT irqs the thread mask is 0 so we can avoid a
	 * conditional in irq_wake_thread().
	 */
	// new->flags: (kmem_cache#30-oX)->flags: 0x14A00, IRQF_ONESHOT: 0x00002000,
	// new->handler: (kmem_cache#30-oX)->handler: exynos4_mct_tick_isr,
	// desc->irq_data.chip->flags: (kmem_cache#28-oX (irq 152))->irq_data.chip->flags: 0,
	// IRQCHIP_ONESHOT_SAFE: 0x20
	// new->flags: (&mct_comp_event_irq)->flags: 0x15200, IRQF_ONESHOT: 0x00002000,
	// new->handler: (&mct_comp_event_irq)->handler: exynos4_mct_comp_isr,
	// desc->irq_data.chip->flags: (kmem_cache#28-oX (irq 347))->irq_data.chip->flags: 0,
	// IRQCHIP_ONESHOT_SAFE: 0x20
	if (new->flags & IRQF_ONESHOT) {
		/*
		 * Unlikely to have 32 resp 64 irqs sharing one line,
		 * but who knows.
		 */
		if (thread_mask == ~0UL) {
			ret = -EBUSY;
			goto out_mask;
		}
		/*
		 * The thread_mask for the action is or'ed to
		 * desc->thread_active to indicate that the
		 * IRQF_ONESHOT thread handler has been woken, but not
		 * yet finished. The bit is cleared when a thread
		 * completes. When all threads of a shared interrupt
		 * line have completed desc->threads_active becomes
		 * zero and the interrupt line is unmasked. See
		 * handle.c:irq_wake_thread() for further information.
		 *
		 * If no thread is woken by primary (hard irq context)
		 * interrupt handlers, then desc->threads_active is
		 * also checked for zero to unmask the irq line in the
		 * affected hard irq flow handlers
		 * (handle_[fasteoi|level]_irq).
		 *
		 * The new action gets the first zero bit of
		 * thread_mask assigned. See the loop above which or's
		 * all existing action->thread_mask bits.
		 */
		new->thread_mask = 1 << ffz(thread_mask);

	} else if (new->handler == irq_default_primary_handler &&
		   !(desc->irq_data.chip->flags & IRQCHIP_ONESHOT_SAFE)) {
		/*
		 * The interrupt was requested with handler = NULL, so
		 * we use the default primary handler for it. But it
		 * does not have the oneshot flag set. In combination
		 * with level interrupts this is deadly, because the
		 * default primary handler just wakes the thread, then
		 * the irq lines is reenabled, but the device still
		 * has the level irq asserted. Rinse and repeat....
		 *
		 * While this works for edge type interrupts, we play
		 * it safe and reject unconditionally because we can't
		 * say for sure which type this interrupt really
		 * has. The type flags are unreliable as the
		 * underlying chip implementation can override them.
		 */
		pr_err("Threaded irq requested with handler=NULL and !ONESHOT for irq %d\n",
		       irq);
		ret = -EINVAL;
		goto out_mask;
	}

	// shared: 0
	// shared: 0
	if (!shared) {
		// &desc->wait_for_threads: &(kmem_cache#28-oX (irq 152))->wait_for_threads
		// &desc->wait_for_threads: &(kmem_cache#28-oX (irq 347))->wait_for_threads
		init_waitqueue_head(&desc->wait_for_threads);

		// init_waitqueue_head에서 한일:
		// &(&(kmem_cache#28-oX (irq 152))->wait_for_threads)->lock을 사용한 spinlock 초기화
		// &(&(kmem_cache#28-oX (irq 152))->wait_for_threads)->task_list를 사용한 list 초기화

		// init_waitqueue_head에서 한일:
		// &(&(kmem_cache#28-oX (irq 347))->wait_for_threads)->lock을 사용한 spinlock 초기화
		// &(&(kmem_cache#28-oX (irq 347))->wait_for_threads)->task_list를 사용한 list 초기화

		/* Setup the type (level, edge polarity) if configured: */
		// new->flags: (kmem_cache#30-oX)->flags: 0x14A00, IRQF_TRIGGER_MASK: 0xF
		// new->flags: (&mct_comp_event_irq)->flags: 0x15200, IRQF_TRIGGER_MASK: 0xF
		if (new->flags & IRQF_TRIGGER_MASK) {
			ret = __irq_set_trigger(desc, irq,
					new->flags & IRQF_TRIGGER_MASK);

			if (ret)
				goto out_mask;
		}

		// &desc->istate: &(kmem_cache#28-oX (irq 152))->istate: 0
		// IRQS_AUTODETECT: 0x00000001, IRQS_SPURIOUS_DISABLED: 0x00000002,
		// IRQS_ONESHOT: 0x00000020, IRQS_WAITING: 0x00000080
		// &desc->istate: &(kmem_cache#28-oX (irq 347))->istate: 0
		// IRQS_AUTODETECT: 0x00000001, IRQS_SPURIOUS_DISABLED: 0x00000002,
		// IRQS_ONESHOT: 0x00000020, IRQS_WAITING: 0x00000080
		desc->istate &= ~(IRQS_AUTODETECT | IRQS_SPURIOUS_DISABLED | \
				  IRQS_ONESHOT | IRQS_WAITING);
		// &desc->istate: &(kmem_cache#28-oX (irq 152))->istate: 0
		// &desc->istate: &(kmem_cache#28-oX (irq 347))->istate: 0

		// &desc->irq_data: &(kmem_cache#28-oX (irq 152))->irq_data, IRQD_IRQ_INPROGRESS: 0x40000
		// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data, IRQD_IRQ_INPROGRESS: 0x40000
		irqd_clear(&desc->irq_data, IRQD_IRQ_INPROGRESS);

		// irqd_clear에서 한일;
		// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x10000

		// irqd_clear에서 한일;
		// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000

		// new->flags: (kmem_cache#30-oX)->flags: 0x14A00, IRQF_PERCPU: 0x00000400
		// new->flags: (&mct_comp_event_irq)->flags: 0x15200, IRQF_PERCPU: 0x00000400
		if (new->flags & IRQF_PERCPU) {
			irqd_set(&desc->irq_data, IRQD_PER_CPU);
			irq_settings_set_per_cpu(desc);
		}

		// new->flags: (kmem_cache#30-oX)->flags: 0x14A00, IRQF_ONESHOT: 0x00002000
		// new->flags: (&mct_comp_event_irq)->flags: 0x15200, IRQF_ONESHOT: 0x00002000
		if (new->flags & IRQF_ONESHOT)
			desc->istate |= IRQS_ONESHOT;

		// desc: kmem_cache#28-oX (irq 152),
		// irq_settings_can_autoenable(kmem_cache#28-oX (irq 152)): 0
		// desc: kmem_cache#28-oX (irq 347),
		// irq_settings_can_autoenable(kmem_cache#28-oX (irq 347)): 1
		if (irq_settings_can_autoenable(desc))
			// desc: kmem_cache#28-oX (irq 347)
			irq_startup(desc, true);

			// irq_startup에서 한일:
			// (kmem_cache#28-oX (irq 347))->depth: 0
			// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000
			//
			// register IESR5의 MCT_G0 bit 를 1 로 write 하여 MCT_G0 의 interrupt 를 enable 시킴
		else
			/* Undo nested disables: */
			// desc->depth: (kmem_cache#28-oX (irq 152))->depth
			desc->depth = 1;
			// desc->depth: (kmem_cache#28-oX (irq 152))->depth: 1

		/* Exclude IRQ from balancing if requested */
		// new->flags: (kmem_cache#30-oX)->flags: 0x14A00, IRQF_NOBALANCING: 0x00000800
		// new->flags: (&mct_comp_event_irq)->flags: 0x15200, IRQF_NOBALANCING: 0x00000800
		if (new->flags & IRQF_NOBALANCING) {
			// desc: kmem_cache#28-oX (irq 152)
			irq_settings_set_no_balancing(desc);

			// irq_settings_set_no_balancing에서 한일:
			// desc->status_use_accessors: (kmem_cache#28-oX (irq 152))->status_use_accessors: 0x3400

// 2015/05/09 종료
// 2015/05/16 시작

			// &desc->irq_data: &(kmem_cache#28-oX (irq 152))->irq_data, IRQD_NO_BALANCING: 0x400
			irqd_set(&desc->irq_data, IRQD_NO_BALANCING);

			// irqd_set에서 한일:
			// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11400
		}

		/* Set default affinity mask once everything is setup */
		// irq: 152, desc: kmem_cache#28-oX (irq 152), setup_affinity(152, kmem_cache#28-oX (irq 152), mask): 0
		// irq: 347, desc: kmem_cache#28-oX (irq 347), setup_affinity(347, kmem_cache#28-oX (irq 347), mask): 0
		setup_affinity(irq, desc, mask);

		// setup_affinity(347)에서 한일:
		// (mask)->bits[0]: 1
		//
		// GICD_ITARGETSR46 값을 모르기 때문에 0x00000000 로
		// 읽히는 것으로 가정하고 GICD_ITARGETSR46에 0x1000000를 write 함
		// CPU interface 0에 interrupt가 발생을 나타냄
		//
		// (&(kmem_cache#28-oX (irq 347))->irq_data)->affinity->bits[0]: 1

	} else if (new->flags & IRQF_TRIGGER_MASK) {
		unsigned int nmsk = new->flags & IRQF_TRIGGER_MASK;
		unsigned int omsk = irq_settings_get_trigger_mask(desc);

		if (nmsk != omsk)
			/* hope the handler works with current  trigger mode */
			pr_warning("irq %d uses trigger mode %u; requested %u\n",
				   irq, nmsk, omsk);
	}

	// new->irq: (kmem_cache#30-oX)->irq, irq: 152
	// new->irq: (&mct_comp_event_irq)->irq, irq: 347
	new->irq = irq;
	// new->irq: (kmem_cache#30-oX)->irq: 152
	// new->irq: (&mct_comp_event_irq)->irq: 347

	// *old_ptr: (kmem_cache#28-oX (irq 152))->action: NULL, new: kmem_cache#30-oX
	// *old_ptr: (kmem_cache#28-oX (irq 347))->action: NULL, new: &mct_comp_event_irq
	*old_ptr = new;
	// *old_ptr: (kmem_cache#28-oX (irq 152))->action: kmem_cache#30-oX
	// *old_ptr: (kmem_cache#28-oX (irq 347))->action: &mct_comp_event_irq

	/* Reset broken irq detection when installing new handler */
	// desc->irq_count: (kmem_cache#28-oX (irq 152))->irq_count
	// desc->irq_count: (kmem_cache#28-oX (irq 347))->irq_count
	desc->irq_count = 0;
	// desc->irq_count: (kmem_cache#28-oX (irq 152))->irq_count: 0
	// desc->irq_count: (kmem_cache#28-oX (irq 347))->irq_count: 0

	// desc->irqs_unhandled: (kmem_cache#28-oX (irq 152))->irqs_unhandled
	// desc->irqs_unhandled: (kmem_cache#28-oX (irq 347))->irqs_unhandled
	desc->irqs_unhandled = 0;
	// desc->irqs_unhandled: (kmem_cache#28-oX (irq 152))->irqs_unhandled: 0
	// desc->irqs_unhandled: (kmem_cache#28-oX (irq 347))->irqs_unhandled: 0

	/*
	 * Check whether we disabled the irq via the spurious handler
	 * before. Reenable it and give it another chance.
	 */
	// shared: 0, desc->istate: (kmem_cache#28-oX (irq 152))->istate: 0,
	// IRQS_SPURIOUS_DISABLED: 0x00000002
	// shared: 0, desc->istate: (kmem_cache#28-oX (irq 347))->istate: 0,
	// IRQS_SPURIOUS_DISABLED: 0x00000002
	if (shared && (desc->istate & IRQS_SPURIOUS_DISABLED)) {
		desc->istate &= ~IRQS_SPURIOUS_DISABLED;
		__enable_irq(desc, irq, false);
	}

	// &desc->lock: (kmem_cache#28-oX (irq 152))->lock
	// &desc->lock: (kmem_cache#28-oX (irq 347))->lock
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	// raw_spin_unlock_irqrestore에서 한일:
	// (kmem_cache#28-oX (irq 152))->lock을 사용하여 spin unlock을 수행하고 flags에 저장된 cpsr을 복원

	// raw_spin_unlock_irqrestore에서 한일:
	// (kmem_cache#28-oX (irq 347))->lock을 사용하여 spin unlock을 수행하고 flags에 저장된 cpsr을 복원

	/*
	 * Strictly no need to wake it up, but hung_task complains
	 * when no hard interrupt wakes the thread up.
	 */
	// new->thread: (kmem_cache#30-oX)->thread: NULL
	// new->thread: (&mct_comp_event_irq)->thread: NULL
	if (new->thread)
		wake_up_process(new->thread);

	// irq: 152, desc: kmem_cache#28-oX (irq 152)
	// irq: 347, desc: kmem_cache#28-oX (irq 347)
	register_irq_proc(irq, desc);

	// new->dir: (kmem_cache#30-oX)->dir
	// new->dir: (&mct_comp_event_irq)->dir
	new->dir = NULL;
	// new->dir: (kmem_cache#30-oX)->dir: NULL
	// new->dir: (&mct_comp_event_irq)->dir: NULL

	// irq: 152, new: kmem_cache#30-oX
	// irq: 347, new: &mct_comp_event_irq
	register_handler_proc(irq, new);
	free_cpumask_var(mask); // null function

	return 0;
	// return 0
	// return 0

mismatch:
	if (!(new->flags & IRQF_PROBE_SHARED)) {
		pr_err("Flags mismatch irq %d. %08x (%s) vs. %08x (%s)\n",
		       irq, new->flags, new->name, old->flags, old->name);
#ifdef CONFIG_DEBUG_SHIRQ
		dump_stack();
#endif
	}
	ret = -EBUSY;

out_mask:
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	free_cpumask_var(mask);

out_thread:
	if (new->thread) {
		struct task_struct *t = new->thread;

		new->thread = NULL;
		kthread_stop(t);
		put_task_struct(t);
	}
out_mput:
	module_put(desc->owner);
	return ret;
}

/**
 *	setup_irq - setup an interrupt
 *	@irq: Interrupt line to setup
 *	@act: irqaction for the interrupt
 *
 * Used to statically setup interrupts in the early boot process.
 */
// ARM10C 20150523
// mct_irqs[0]: 347, &mct_comp_event_irq
int setup_irq(unsigned int irq, struct irqaction *act)
{
	int retval;

	// irq: 347, irq_to_desc(347): kmem_cache#28-oX (irq 347)
	struct irq_desc *desc = irq_to_desc(irq);
	// desc: kmem_cache#28-oX (irq 347)

	// desc: kmem_cache#28-oX (irq 347),
	// irq_settings_is_per_cpu_devid(kmem_cache#28-oX (irq 347)): 0
	if (WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		return -EINVAL;

	// desc: kmem_cache#28-oX (irq 347)
	chip_bus_lock(desc);

	// irq: 347, desc: kmem_cache#28-oX (irq 347), act: &mct_comp_event_irq
	// __setup_irq(347, kmem_cache#28-oX (irq 347), &mct_comp_event_irq): 0
	retval = __setup_irq(irq, desc, act);
	// retval: 0

	// __setup_irq에서 한일:
	// &(&(kmem_cache#28-oX (irq 347))->wait_for_threads)->lock을 사용한 spinlock 초기화
	// &(&(kmem_cache#28-oX (irq 347))->wait_for_threads)->task_list를 사용한 list 초기화
	// &(kmem_cache#28-oX (irq 347))->istate: 0
	// (kmem_cache#28-oX (irq 347))->depth: 0
	// (kmem_cache#28-oX (irq 347))->action: &mct_comp_event_irq
	// (kmem_cache#28-oX (irq 347))->irq_count: 0
	// (kmem_cache#28-oX (irq 347))->irqs_unhandled: 0
	//
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->affinity->bits[0]: 1
	//
	// register IESR5의 MCT_G0 bit 를 1 로 write 하여 MCT_G0 의 interrupt 를 enable 시킴
	//
	// GICD_ITARGETSR46 값을 모르기 때문에 0x00000000 로
	// 읽히는 것으로 가정하고 GICD_ITARGETSR46에 0x1000000를 write 함
	// CPU interface 0에 interrupt가 발생을 나타냄
	//
	// struct irqaction 멤버 값 세팅
	// (&mct_comp_event_irq)->irq: 347
	// (&mct_comp_event_irq)->dir: NULL

	// desc: kmem_cache#28-oX (irq 347)
	chip_bus_sync_unlock(desc);

	// retval: 0
	return retval;
	// return 0
}
EXPORT_SYMBOL_GPL(setup_irq);

/*
 * Internal function to unregister an irqaction - used to free
 * regular and special interrupts that are part of the architecture.
 */
static struct irqaction *__free_irq(unsigned int irq, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action, **action_ptr;
	unsigned long flags;

	WARN(in_interrupt(), "Trying to free IRQ %d from IRQ context!\n", irq);

	if (!desc)
		return NULL;

	raw_spin_lock_irqsave(&desc->lock, flags);

	/*
	 * There can be multiple actions per IRQ descriptor, find the right
	 * one based on the dev_id:
	 */
	action_ptr = &desc->action;
	for (;;) {
		action = *action_ptr;

		if (!action) {
			WARN(1, "Trying to free already-free IRQ %d\n", irq);
			raw_spin_unlock_irqrestore(&desc->lock, flags);

			return NULL;
		}

		if (action->dev_id == dev_id)
			break;
		action_ptr = &action->next;
	}

	/* Found it - now remove it from the list of entries: */
	*action_ptr = action->next;

	/* If this was the last handler, shut down the IRQ line: */
	if (!desc->action)
		irq_shutdown(desc);

#ifdef CONFIG_SMP
	/* make sure affinity_hint is cleaned up */
	if (WARN_ON_ONCE(desc->affinity_hint))
		desc->affinity_hint = NULL;
#endif

	raw_spin_unlock_irqrestore(&desc->lock, flags);

	unregister_handler_proc(irq, action);

	/* Make sure it's not being used on another CPU: */
	synchronize_irq(irq);

#ifdef CONFIG_DEBUG_SHIRQ
	/*
	 * It's a shared IRQ -- the driver ought to be prepared for an IRQ
	 * event to happen even now it's being freed, so let's make sure that
	 * is so by doing an extra call to the handler ....
	 *
	 * ( We do this after actually deregistering it, to make sure that a
	 *   'real' IRQ doesn't run in * parallel with our fake. )
	 */
	if (action->flags & IRQF_SHARED) {
		local_irq_save(flags);
		action->handler(irq, dev_id);
		local_irq_restore(flags);
	}
#endif

	if (action->thread) {
		kthread_stop(action->thread);
		put_task_struct(action->thread);
	}

	module_put(desc->owner);
	return action;
}

/**
 *	remove_irq - free an interrupt
 *	@irq: Interrupt line to free
 *	@act: irqaction for the interrupt
 *
 * Used to remove interrupts statically setup by the early boot process.
 */
void remove_irq(unsigned int irq, struct irqaction *act)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc && !WARN_ON(irq_settings_is_per_cpu_devid(desc)))
	    __free_irq(irq, act->dev_id);
}
EXPORT_SYMBOL_GPL(remove_irq);

/**
 *	free_irq - free an interrupt allocated with request_irq
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Remove an interrupt handler. The handler is removed and if the
 *	interrupt line is no longer in use by any driver it is disabled.
 *	On a shared IRQ the caller must ensure the interrupt is disabled
 *	on the card it drives before calling this function. The function
 *	does not return until any executing interrupts for this IRQ
 *	have completed.
 *
 *	This function must not be called from interrupt context.
 */
void free_irq(unsigned int irq, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc || WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		return;

#ifdef CONFIG_SMP
	if (WARN_ON(desc->affinity_notify))
		desc->affinity_notify = NULL;
#endif

	chip_bus_lock(desc);
	kfree(__free_irq(irq, dev_id));
	chip_bus_sync_unlock(desc);
}
EXPORT_SYMBOL(free_irq);

/**
 *	request_threaded_irq - allocate an interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs.
 *		  Primary handler for threaded interrupts
 *		  If NULL and thread_fn != NULL the default
 *		  primary handler is installed
 *	@thread_fn: Function called from the irq handler thread
 *		    If NULL, no irq thread is created
 *	@irqflags: Interrupt type flags
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt line and IRQ handling. From the point this
 *	call is made your handler function may be invoked. Since
 *	your handler function must clear any interrupt the board
 *	raises, you must take care both to initialise your hardware
 *	and to set up the interrupt handler in the right order.
 *
 *	If you want to set up a threaded irq handler for your device
 *	then you need to supply @handler and @thread_fn. @handler is
 *	still called in hard interrupt context and has to check
 *	whether the interrupt originates from the device. If yes it
 *	needs to disable the interrupt on the device and return
 *	IRQ_WAKE_THREAD which will wake up the handler thread and run
 *	@thread_fn. This split handler design is necessary to support
 *	shared interrupts.
 *
 *	Dev_id must be globally unique. Normally the address of the
 *	device data structure is used as the cookie. Since the handler
 *	receives this value it makes sense to use it.
 *
 *	If your interrupt is shared you must pass a non NULL dev_id
 *	as this is required when freeing the interrupt.
 *
 *	Flags:
 *
 *	IRQF_SHARED		Interrupt is shared
 *	IRQF_TRIGGER_*		Specify active edge(s) or level
 *
 */
// ARM10C 20150509
// irq: 152, handler: exynos4_mct_tick_isr, NULL, flags: 0x14A00, name: "mct_tick0", dev: [pcp0] &percpu_mct_tick
int request_threaded_irq(unsigned int irq, irq_handler_t handler,
			 irq_handler_t thread_fn, unsigned long irqflags,
			 const char *devname, void *dev_id)
{
	struct irqaction *action;
	struct irq_desc *desc;
	int retval;

	/*
	 * Sanity-check: shared interrupts must pass in a real dev-ID,
	 * otherwise we'll have trouble later trying to figure out
	 * which interrupt is which (messes up the interrupt freeing
	 * logic etc).
	 */
	// irqflags: 0x14A00, IRQF_SHARED: 0x00000080, dev_id: [pcp0] &percpu_mct_tick
	if ((irqflags & IRQF_SHARED) && !dev_id)
		return -EINVAL;

	// irq: 152, irq_to_desc(152): kmem_cache#28-oX (irq 152)
	desc = irq_to_desc(irq);
	// desc: kmem_cache#28-oX (irq 152)

	// desc: kmem_cache#28-oX (irq 152)
	if (!desc)
		return -EINVAL;

	// desc: kmem_cache#28-oX (irq 152), irq_settings_can_request(kmem_cache#28-oX (irq 152)): 1
	// irq_settings_is_per_cpu_devid(kmem_cache#28-oX (irq 152)): 0
	if (!irq_settings_can_request(desc) ||
	    WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		return -EINVAL;

	// handler: exynos4_mct_tick_isr
	if (!handler) {
		if (!thread_fn)
			return -EINVAL;
		handler = irq_default_primary_handler;
	}

	// sizeof(struct irqaction): 48 bytes, GFP_KERNEL: 0xD0, kzalloc(48, 0xD0): kmem_cache#30-oX
	action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
	// action: kmem_cache#30-oX

	// action: kmem_cache#30-oX
	if (!action)
		return -ENOMEM;

	// action->handler: (kmem_cache#30-oX)->handler, handler: exynos4_mct_tick_isr
	action->handler = handler;
	// action->handler: (kmem_cache#30-oX)->handler: exynos4_mct_tick_isr

	// action->thread_fn: (kmem_cache#30-oX)->thread_fn, thread_fn: NULL
	action->thread_fn = thread_fn;
	// action->thread_fn: (kmem_cache#30-oX)->thread_fn: NULL

	// action->flags: (kmem_cache#30-oX)->flags, irqflags: 0x14A00
	action->flags = irqflags;
	// action->flags: (kmem_cache#30-oX)->flags: 0x14A00

	// action->name: (kmem_cache#30-oX)->name, devname: "mct_tick0"
	action->name = devname;
	// action->name: (kmem_cache#30-oX)->name: "mct_tick0"

	// action->dev_id: (kmem_cache#30-oX)->dev_id, dev_id: [pcp0] &percpu_mct_tick
	action->dev_id = dev_id;
	// action->dev_id: (kmem_cache#30-oX)->dev_id: [pcp0] &percpu_mct_tick

	// desc: kmem_cache#28-oX (irq 152)
	chip_bus_lock(desc);

	// irq: 152, desc: kmem_cache#28-oX (irq 152), action: kmem_cache#30-oX
	// __setup_irq(152, kmem_cache#28-oX (irq 152), kmem_cache#30-oX): 0
	retval = __setup_irq(irq, desc, action);
	// retval: 0

	// __setup_irq에서 한일:
	// &(&(kmem_cache#28-oX (irq 152))->wait_for_threads)->lock을 사용한 spinlock 초기화
	// &(&(kmem_cache#28-oX (irq 152))->wait_for_threads)->task_list를 사용한 list 초기화
	// (kmem_cache#28-oX (irq 152))->istate: 0
	// (kmem_cache#28-oX (irq 152))->depth: 1
	// (kmem_cache#28-oX (irq 152))->action: kmem_cache#30-oX (irqaction)
	// (kmem_cache#28-oX (irq 152))->status_use_accessors: 0x3400
	// (kmem_cache#28-oX (irq 152))->irq_count: 0
	// (kmem_cache#28-oX (irq 152))->irqs_unhandled: 0
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11400
	//
	// struct irqaction 멤버 값 세팅
	// (kmem_cache#30-oX)->irq: 152
	// (kmem_cache#30-oX)->dir: NULL

	// desc: kmem_cache#28-oX (irq 152)
	chip_bus_sync_unlock(desc);

	// retval: 0
	if (retval)
		kfree(action);

#ifdef CONFIG_DEBUG_SHIRQ_FIXME // CONFIG_DEBUG_SHIRQ_FIXME=n
	if (!retval && (irqflags & IRQF_SHARED)) {
		/*
		 * It's a shared IRQ -- the driver ought to be prepared for it
		 * to happen immediately, so let's make sure....
		 * We disable the irq to make sure that a 'real' IRQ doesn't
		 * run in parallel with our fake.
		 */
		unsigned long flags;

		disable_irq(irq);
		local_irq_save(flags);

		handler(irq, dev_id);

		local_irq_restore(flags);
		enable_irq(irq);
	}
#endif
	// retval: 0
	return retval;
	// return 0
}
EXPORT_SYMBOL(request_threaded_irq);

/**
 *	request_any_context_irq - allocate an interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs.
 *		  Threaded handler for threaded interrupts.
 *	@flags: Interrupt type flags
 *	@name: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt line and IRQ handling. It selects either a
 *	hardirq or threaded handling method depending on the
 *	context.
 *
 *	On failure, it returns a negative value. On success,
 *	it returns either IRQC_IS_HARDIRQ or IRQC_IS_NESTED.
 */
int request_any_context_irq(unsigned int irq, irq_handler_t handler,
			    unsigned long flags, const char *name, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int ret;

	if (!desc)
		return -EINVAL;

	if (irq_settings_is_nested_thread(desc)) {
		ret = request_threaded_irq(irq, NULL, handler,
					   flags, name, dev_id);
		return !ret ? IRQC_IS_NESTED : ret;
	}

	ret = request_irq(irq, handler, flags, name, dev_id);
	return !ret ? IRQC_IS_HARDIRQ : ret;
}
EXPORT_SYMBOL_GPL(request_any_context_irq);

void enable_percpu_irq(unsigned int irq, unsigned int type)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_PERCPU);

	if (!desc)
		return;

	type &= IRQ_TYPE_SENSE_MASK;
	if (type != IRQ_TYPE_NONE) {
		int ret;

		ret = __irq_set_trigger(desc, irq, type);

		if (ret) {
			WARN(1, "failed to set type for IRQ%d\n", irq);
			goto out;
		}
	}

	irq_percpu_enable(desc, cpu);
out:
	irq_put_desc_unlock(desc, flags);
}
EXPORT_SYMBOL_GPL(enable_percpu_irq);

void disable_percpu_irq(unsigned int irq)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_PERCPU);

	if (!desc)
		return;

	irq_percpu_disable(desc, cpu);
	irq_put_desc_unlock(desc, flags);
}
EXPORT_SYMBOL_GPL(disable_percpu_irq);

/*
 * Internal function to unregister a percpu irqaction.
 */
static struct irqaction *__free_percpu_irq(unsigned int irq, void __percpu *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	unsigned long flags;

	WARN(in_interrupt(), "Trying to free IRQ %d from IRQ context!\n", irq);

	if (!desc)
		return NULL;

	raw_spin_lock_irqsave(&desc->lock, flags);

	action = desc->action;
	if (!action || action->percpu_dev_id != dev_id) {
		WARN(1, "Trying to free already-free IRQ %d\n", irq);
		goto bad;
	}

	if (!cpumask_empty(desc->percpu_enabled)) {
		WARN(1, "percpu IRQ %d still enabled on CPU%d!\n",
		     irq, cpumask_first(desc->percpu_enabled));
		goto bad;
	}

	/* Found it - now remove it from the list of entries: */
	desc->action = NULL;

	raw_spin_unlock_irqrestore(&desc->lock, flags);

	unregister_handler_proc(irq, action);

	module_put(desc->owner);
	return action;

bad:
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return NULL;
}

/**
 *	remove_percpu_irq - free a per-cpu interrupt
 *	@irq: Interrupt line to free
 *	@act: irqaction for the interrupt
 *
 * Used to remove interrupts statically setup by the early boot process.
 */
void remove_percpu_irq(unsigned int irq, struct irqaction *act)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc && irq_settings_is_per_cpu_devid(desc))
	    __free_percpu_irq(irq, act->percpu_dev_id);
}

/**
 *	free_percpu_irq - free an interrupt allocated with request_percpu_irq
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Remove a percpu interrupt handler. The handler is removed, but
 *	the interrupt line is not disabled. This must be done on each
 *	CPU before calling this function. The function does not return
 *	until any executing interrupts for this IRQ have completed.
 *
 *	This function must not be called from interrupt context.
 */
void free_percpu_irq(unsigned int irq, void __percpu *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc || !irq_settings_is_per_cpu_devid(desc))
		return;

	chip_bus_lock(desc);
	kfree(__free_percpu_irq(irq, dev_id));
	chip_bus_sync_unlock(desc);
}

/**
 *	setup_percpu_irq - setup a per-cpu interrupt
 *	@irq: Interrupt line to setup
 *	@act: irqaction for the interrupt
 *
 * Used to statically setup per-cpu interrupts in the early boot process.
 */
int setup_percpu_irq(unsigned int irq, struct irqaction *act)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int retval;

	if (!desc || !irq_settings_is_per_cpu_devid(desc))
		return -EINVAL;
	chip_bus_lock(desc);
	retval = __setup_irq(irq, desc, act);
	chip_bus_sync_unlock(desc);

	return retval;
}

/**
 *	request_percpu_irq - allocate a percpu interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs.
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A percpu cookie passed back to the handler function
 *
 *	This call allocates interrupt resources, but doesn't
 *	automatically enable the interrupt. It has to be done on each
 *	CPU using enable_percpu_irq().
 *
 *	Dev_id must be globally unique. It is a per-cpu variable, and
 *	the handler gets called with the interrupted CPU's instance of
 *	that variable.
 */
int request_percpu_irq(unsigned int irq, irq_handler_t handler,
		       const char *devname, void __percpu *dev_id)
{
	struct irqaction *action;
	struct irq_desc *desc;
	int retval;

	if (!dev_id)
		return -EINVAL;

	desc = irq_to_desc(irq);
	if (!desc || !irq_settings_can_request(desc) ||
	    !irq_settings_is_per_cpu_devid(desc))
		return -EINVAL;

	action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->flags = IRQF_PERCPU | IRQF_NO_SUSPEND;
	action->name = devname;
	action->percpu_dev_id = dev_id;

	chip_bus_lock(desc);
	retval = __setup_irq(irq, desc, action);
	chip_bus_sync_unlock(desc);

	if (retval)
		kfree(action);

	return retval;
}
