/*
 * linux/kernel/irq/resend.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner
 *
 * This file contains the IRQ-resend code
 *
 * If the interrupt is waiting to be processed, we try to re-run it.
 * We can't directly run it from here since the caller might be in an
 * interrupt-protected region. Not all irq controller chips can
 * retrigger interrupts at the hardware level, so in those cases
 * we allow the resending of IRQs via a tasklet.
 */

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/interrupt.h>

#include "internals.h"

#ifdef CONFIG_HARDIRQS_SW_RESEND

/* Bitmap to handle software resend of interrupts: */
static DECLARE_BITMAP(irqs_resend, IRQ_BITMAP_BITS);

/*
 * Run software resends of IRQ's
 */
static void resend_irqs(unsigned long arg)
{
	struct irq_desc *desc;
	int irq;

	while (!bitmap_empty(irqs_resend, nr_irqs)) {
		irq = find_first_bit(irqs_resend, nr_irqs);
		clear_bit(irq, irqs_resend);
		desc = irq_to_desc(irq);
		local_irq_disable();
		desc->handle_irq(irq, desc);
		local_irq_enable();
	}
}

/* Tasklet to handle resend: */
static DECLARE_TASKLET(resend_tasklet, resend_irqs, 0);

#endif

/*
 * IRQ resend
 *
 * Is called with interrupts disabled and desc->lock held.
 */
// ARM10C 20141220
// desc: kmem_cache#28-oX (irq 32),
// desc->irq_data.irq: (kmem_cache#28-oX (irq 32))->irq_data.irq: 32
// ARM10C 20150523
// desc: kmem_cache#28-oX (irq 347),
// desc->irq_data.irq: (kmem_cache#28-oX (irq 347))->irq_data.irq: 347
void check_irq_resend(struct irq_desc *desc, unsigned int irq)
{
	/*
	 * We do not resend level type interrupts. Level type
	 * interrupts are resent by hardware when they are still
	 * active. Clear the pending bit so suspend/resume does not
	 * get confused.
	 */
	// desc: kmem_cache#28-oX (irq 32),
	// irq_settings_is_level(kmem_cache#28-oX (irq 32)): 0
	// desc: kmem_cache#28-oX (irq 347),
	// irq_settings_is_level(kmem_cache#28-oX (irq 347)): 0
	if (irq_settings_is_level(desc)) {
		desc->istate &= ~IRQS_PENDING;
		return;
	}

	// desc->istate: (kmem_cache#28-oX (irq 32))->istate: 0, IRQS_REPLAY: 0x00000040
	// desc->istate: (kmem_cache#28-oX (irq 347))->istate: 0, IRQS_REPLAY: 0x00000040
	if (desc->istate & IRQS_REPLAY)
		return;

	// desc->istate: (kmem_cache#28-oX (irq 32))->istate: 0, IRQS_PENDING: 0x00000200
	// desc->istate: (kmem_cache#28-oX (irq 347))->istate: 0, IRQS_PENDING: 0x00000200
	if (desc->istate & IRQS_PENDING) {
		desc->istate &= ~IRQS_PENDING;
		desc->istate |= IRQS_REPLAY;

		if (!desc->irq_data.chip->irq_retrigger ||
		    !desc->irq_data.chip->irq_retrigger(&desc->irq_data)) {
#ifdef CONFIG_HARDIRQS_SW_RESEND // CONFIG_HARDIRQS_SW_RESEND=y
			/*
			 * If the interrupt has a parent irq and runs
			 * in the thread context of the parent irq,
			 * retrigger the parent.
			 */
			if (desc->parent_irq &&
			    irq_settings_is_nested_thread(desc))
				irq = desc->parent_irq;
			/* Set it pending and activate the softirq: */
			set_bit(irq, irqs_resend);
			tasklet_schedule(&resend_tasklet);
#endif
		}
	}
}
