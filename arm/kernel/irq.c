/*
 *  linux/arch/arm/kernel/irq.c
 *
 *  Copyright (C) 1992 Linus Torvalds
 *  Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 *
 *  Support for Dynamic Tick Timer Copyright (C) 2004-2005 Nokia Corporation.
 *  Dynamic Tick Timer written by Tony Lindgren <tony@atomide.com> and
 *  Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file contains the code used by various IRQ handling routines:
 *  asking for different IRQ's should be done through these routines
 *  instead of just grabbing them. Thus setups with different IRQ numbers
 *  shouldn't result in any weird surprises, and installing new handlers
 *  should be easier.
 *
 *  IRQ's are in fact implemented a bit like signal handlers for the kernel.
 *  Naturally it's not a 1:1 relation, but there are similarities.
 */
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/kallsyms.h>
#include <linux/proc_fs.h>
#include <linux/export.h>

#include <asm/exception.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

unsigned long irq_err_count;

int arch_show_interrupts(struct seq_file *p, int prec)
{
#ifdef CONFIG_FIQ
	show_fiq_list(p, prec);
#endif
#ifdef CONFIG_SMP
	show_ipi_list(p, prec);
#endif
	seq_printf(p, "%*s: %10lu\n", prec, "Err", irq_err_count);
	return 0;
}

/*
 * handle_IRQ handles all hardware IRQ's.  Decoded IRQs should
 * not come via this function.  Instead, they should provide their
 * own 'handler'.  Used by platform code implementing C-based 1st
 * level decoding.
 */
// ARM10C 20141227
// irqnr: 63, regs: svc_entry에서 만든 struct pt_regs의 시작 주소
void handle_IRQ(unsigned int irq, struct pt_regs *regs)
{
	// regs: svc_entry에서 만든 struct pt_regs의 시작 주소
	// set_irq_regs(svc_entry에서 만든 struct pt_regs의 시작 주소): [pcp0] irq 발생 전의 regs 값
	struct pt_regs *old_regs = set_irq_regs(regs);
	// old_regs: [pcp0] irq 발생 전의 regs 값
	
	// set_irq_regs에서 한일:
	// [pcp0] __irq_regs에 svc_entry에서 만든 struct pt_regs의 시작 주소 를 저장

	irq_enter();

	/*
	 * Some hardware gives randomly wrong interrupts.  Rather
	 * than crashing, do something sensible.
	 */
	if (unlikely(irq >= nr_irqs)) {
		if (printk_ratelimit())
			printk(KERN_WARNING "Bad IRQ%u\n", irq);
		ack_bad_irq(irq);
	} else {
		generic_handle_irq(irq);
	}

	irq_exit();
	set_irq_regs(old_regs);
}

/*
 * asm_do_IRQ is the interface to be used from assembly code.
 */
asmlinkage void __exception_irq_entry
asm_do_IRQ(unsigned int irq, struct pt_regs *regs)
{
	handle_IRQ(irq, regs);
}

// ARM10C 20141122
// irq: 16, 0x5
// ARM10C 20141122
// irq: 32, 0x5
// ARM10C 20141213
// irq: 160, 0x3
void set_irq_flags(unsigned int irq, unsigned int iflags)
{
	// IRQ_NOREQUEST: 0x800, IRQ_NOPROBE: 0x400, IRQ_NOAUTOEN: 0x1000
	// IRQ_NOREQUEST: 0x800, IRQ_NOPROBE: 0x400, IRQ_NOAUTOEN: 0x1000
	// IRQ_NOREQUEST: 0x800, IRQ_NOPROBE: 0x400, IRQ_NOAUTOEN: 0x1000
	unsigned long clr = 0, set = IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;
	// clr: 0, set: 0x1c00
	// clr: 0, set: 0x1c00
	// clr: 0, set: 0x1c00

	// irq; 16, nr_irqs: 160
	// irq; 32, nr_irqs: 160
	// irq; 160, nr_irqs: 416
	if (irq >= nr_irqs) {
		printk(KERN_ERR "Trying to set irq flags for IRQ%d\n", irq);
		return;
	}

	// iflags: 0x5, IRQF_VALID: 1
	// iflags: 0x5, IRQF_VALID: 1
	// iflags: 0x3, IRQF_VALID: 1
	if (iflags & IRQF_VALID)
		// clr: 0, IRQ_NOREQUEST: 0x800
		// clr: 0, IRQ_NOREQUEST: 0x800
		// clr: 0, IRQ_NOREQUEST: 0x800
		clr |= IRQ_NOREQUEST;
		// clr: 0x800
		// clr: 0x800
		// clr: 0x800

	// iflags: 0x5, IRQF_PROBE: 0x2
	// iflags: 0x5, IRQF_PROBE: 0x2
	// iflags: 0x3, IRQF_PROBE: 0x2
	if (iflags & IRQF_PROBE)
		// clr: 0x800, IRQ_NOPROBE: 0x400
		clr |= IRQ_NOPROBE;
		// clr: 0xc00

	// iflags: 0x5, IRQF_NOAUTOEN: 0x4
	// iflags: 0x5, IRQF_NOAUTOEN: 0x4
	// iflags: 0x3, IRQF_NOAUTOEN: 0x4
	if (!(iflags & IRQF_NOAUTOEN))
		// clr: 0xc00, IRQ_NOAUTOEN: 0x1000
		clr |= IRQ_NOAUTOEN;
		// clr: 0x1c00

	/* Order is clear bits in "clr" then set bits in "set" */
	// irq: 16, clr: 0x800, set: 0x1c00
	// irq: 32, clr: 0x800, set: 0x1c00
	// irq: 160, clr: 0x1c00, set: 0x1c00
	irq_modify_status(irq, clr, set & ~clr);

	// irq_modify_status(16)에서 한일:
	// (kmem_cache#28-oX (irq 16))->status_use_accessors: 0x31600
	// (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10800

	// irq_modify_status(32)에서 한일:
	// (kmem_cache#28-oX (irq 32))->status_use_accessors: 0x1400
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0x10000

	// irq_modify_status(160)에서 한일:
	// (kmem_cache#28-oX (irq 160))->status_use_accessors: 0
	// (&(kmem_cache#28-oX (irq 160))->irq_data)->state_use_accessors: 0x10000
}
EXPORT_SYMBOL_GPL(set_irq_flags);

// ARM10C 20141004
void __init init_IRQ(void)
{
	// CONFIG_OF=y, machine_desc->init_irq: __mach_desc_EXYNOS5_DT.init_irq: 0
	if (IS_ENABLED(CONFIG_OF) && !machine_desc->init_irq)
		irqchip_init();
	else
		machine_desc->init_irq();
}

#ifdef CONFIG_MULTI_IRQ_HANDLER // CONFIG_MULTI_IRQ_HANDLER=y
// ARM10C 20141129
// gic_handle_irq
void __init set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
	// handle_arch_irq: NULL
	if (handle_arch_irq)
		return;

	// handle_irq: gic_handle_irq
	handle_arch_irq = handle_irq;
	// handle_arch_irq: gic_handle_irq
}
#endif

#ifdef CONFIG_SPARSE_IRQ // CONFIG_SPARSE_IRQ=y
// ARM10C 20141004
int __init arch_probe_nr_irqs(void)
{
	// machine_desc->nr_irqs: __mach_desc_EXYNOS5_DT.nr_irqs: 0, NR_IRQS: 16
	nr_irqs = machine_desc->nr_irqs ? machine_desc->nr_irqs : NR_IRQS;
	// nr_irqs: 16

	// nr_irqs: 16
	return nr_irqs;
	// return 16
}
#endif

#ifdef CONFIG_HOTPLUG_CPU

static bool migrate_one_irq(struct irq_desc *desc)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	const struct cpumask *affinity = d->affinity;
	struct irq_chip *c;
	bool ret = false;

	/*
	 * If this is a per-CPU interrupt, or the affinity does not
	 * include this CPU, then we have nothing to do.
	 */
	if (irqd_is_per_cpu(d) || !cpumask_test_cpu(smp_processor_id(), affinity))
		return false;

	if (cpumask_any_and(affinity, cpu_online_mask) >= nr_cpu_ids) {
		affinity = cpu_online_mask;
		ret = true;
	}

	c = irq_data_get_irq_chip(d);
	if (!c->irq_set_affinity)
		pr_debug("IRQ%u: unable to set affinity\n", d->irq);
	else if (c->irq_set_affinity(d, affinity, true) == IRQ_SET_MASK_OK && ret)
		cpumask_copy(d->affinity, affinity);

	return ret;
}

/*
 * The current CPU has been marked offline.  Migrate IRQs off this CPU.
 * If the affinity settings do not allow other CPUs, force them onto any
 * available CPU.
 *
 * Note: we must iterate over all IRQs, whether they have an attached
 * action structure or not, as we need to get chained interrupts too.
 */
void migrate_irqs(void)
{
	unsigned int i;
	struct irq_desc *desc;
	unsigned long flags;

	local_irq_save(flags);

	for_each_irq_desc(i, desc) {
		bool affinity_broken;

		raw_spin_lock(&desc->lock);
		affinity_broken = migrate_one_irq(desc);
		raw_spin_unlock(&desc->lock);

		if (affinity_broken && printk_ratelimit())
			pr_warning("IRQ%u no longer affine to CPU%u\n", i,
				smp_processor_id());
	}

	local_irq_restore(flags);
}
#endif /* CONFIG_HOTPLUG_CPU */
