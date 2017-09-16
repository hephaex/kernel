/*
 * IRQ subsystem internal functions and variables:
 *
 * Do not ever include this file from anything else than
 * kernel/irq/. Do not even think about using any information outside
 * of this file for your non core code.
 */
#include <linux/irqdesc.h>

#ifdef CONFIG_SPARSE_IRQ // CONFIG_SPARSE_IRQ=y
// ARM10C 20141004
// ARM10C 20141115
// NR_IRQS: 16
// IRQ_BITMAP_BITS: 8212
# define IRQ_BITMAP_BITS	(NR_IRQS + 8196)
#else
# define IRQ_BITMAP_BITS	NR_IRQS
#endif

// ARM10C 20141220
// ARM10C 20150509
#define istate core_internal_state__do_not_mess_with_it

extern bool noirqdebug;

/*
 * Bits used by threaded handlers:
 * IRQTF_RUNTHREAD - signals that the interrupt handler thread should run
 * IRQTF_WARNED    - warning "IRQ_WAKE_THREAD w/o thread_fn" has been printed
 * IRQTF_AFFINITY  - irq thread is requested to adjust affinity
 * IRQTF_FORCED_THREAD  - irq action is force threaded
 */
enum {
	IRQTF_RUNTHREAD,
	IRQTF_WARNED,
	IRQTF_AFFINITY,
	IRQTF_FORCED_THREAD,
};

/*
 * Bit masks for desc->state
 *
 * IRQS_AUTODETECT		- autodetection in progress
 * IRQS_SPURIOUS_DISABLED	- was disabled due to spurious interrupt
 *				  detection
 * IRQS_POLL_INPROGRESS		- polling in progress
 * IRQS_ONESHOT			- irq is not unmasked in primary handler
 * IRQS_REPLAY			- irq is replayed
 * IRQS_WAITING			- irq is waiting
 * IRQS_PENDING			- irq is pending and replayed later
 * IRQS_SUSPENDED		- irq is suspended
 */
// ARM10C 20141220
// ARM10C 20150509
// ARM10C 20150516
enum {
	// IRQS_AUTODETECT: 0x00000001
	IRQS_AUTODETECT		= 0x00000001,
	// IRQS_SPURIOUS_DISABLED: 0x00000002
	IRQS_SPURIOUS_DISABLED	= 0x00000002,
	IRQS_POLL_INPROGRESS	= 0x00000008,
	// IRQS_ONESHOT: 0x00000020
	IRQS_ONESHOT		= 0x00000020,
	// IRQS_REPLAY: 0x00000040
	IRQS_REPLAY		= 0x00000040,
	// IRQS_WAITING: 0x00000080
	IRQS_WAITING		= 0x00000080,
	// IRQS_PENDING: 0x00000200
	IRQS_PENDING		= 0x00000200,
	IRQS_SUSPENDED		= 0x00000800,
};

#include "debug.h"
#include "settings.h"

// ARM10C 20150328
// data: &(kmem_cache#28-oX (irq 152))->irq_data
// ARM10C 20150523
// data: &(kmem_cache#28-oX (irq 347))->irq_data
#define irq_data_to_desc(data)	container_of(data, struct irq_desc, irq_data)

extern int __irq_set_trigger(struct irq_desc *desc, unsigned int irq,
		unsigned long flags);
extern void __disable_irq(struct irq_desc *desc, unsigned int irq, bool susp);
extern void __enable_irq(struct irq_desc *desc, unsigned int irq, bool resume);

extern int irq_startup(struct irq_desc *desc, bool resend);
extern void irq_shutdown(struct irq_desc *desc);
extern void irq_enable(struct irq_desc *desc);
extern void irq_disable(struct irq_desc *desc);
extern void irq_percpu_enable(struct irq_desc *desc, unsigned int cpu);
extern void irq_percpu_disable(struct irq_desc *desc, unsigned int cpu);
extern void mask_irq(struct irq_desc *desc);
extern void unmask_irq(struct irq_desc *desc);

extern void init_kstat_irqs(struct irq_desc *desc, int node, int nr);

irqreturn_t handle_irq_event_percpu(struct irq_desc *desc, struct irqaction *action);
irqreturn_t handle_irq_event(struct irq_desc *desc);

/* Resending of interrupts :*/
void check_irq_resend(struct irq_desc *desc, unsigned int irq);
bool irq_wait_for_poll(struct irq_desc *desc);

#ifdef CONFIG_PROC_FS // CONFIG_PROC_FS=y
extern void register_irq_proc(unsigned int irq, struct irq_desc *desc);
extern void unregister_irq_proc(unsigned int irq, struct irq_desc *desc);
extern void register_handler_proc(unsigned int irq, struct irqaction *action);
extern void unregister_handler_proc(unsigned int irq, struct irqaction *action);
#else
static inline void register_irq_proc(unsigned int irq, struct irq_desc *desc) { }
static inline void unregister_irq_proc(unsigned int irq, struct irq_desc *desc) { }
static inline void register_handler_proc(unsigned int irq,
					 struct irqaction *action) { }
static inline void unregister_handler_proc(unsigned int irq,
					   struct irqaction *action) { }
#endif

extern int irq_select_affinity_usr(unsigned int irq, struct cpumask *mask);

extern void irq_set_thread_affinity(struct irq_desc *desc);

extern int irq_do_set_affinity(struct irq_data *data,
			       const struct cpumask *dest, bool force);

/* Inline functions for support of irq chips on slow busses */
// ARM10C 20141122
// desc: kmem_cache#28-oX (irq 16)
// ARM10C 20150509
// desc: kmem_cache#28-oX (irq 152)
// ARM10C 20150523
// desc: kmem_cache#28-oX (irq 347)
static inline void chip_bus_lock(struct irq_desc *desc)
{
	// desc->irq_data.chip->irq_bus_lock: (kmem_cache#28-oX (irq 16))->irq_data.chip->irq_bus_lock: NULL
	// desc->irq_data.chip->irq_bus_lock: (kmem_cache#28-oX (irq 152))->irq_data.chip->irq_bus_lock: NULL
	// desc->irq_data.chip->irq_bus_lock: (kmem_cache#28-oX (irq 347))->irq_data.chip->irq_bus_lock: NULL
	if (unlikely(desc->irq_data.chip->irq_bus_lock))
		desc->irq_data.chip->irq_bus_lock(&desc->irq_data);
}

// ARM10C 20150516
// desc: kmem_cache#28-oX (irq 152)
// ARM10C 20150523
// desc: kmem_cache#28-oX (irq 347)
static inline void chip_bus_sync_unlock(struct irq_desc *desc)
{
	// desc->irq_data.chip->irq_bus_sync_unlock: (kmem_cache#28-oX (irq 152))->irq_data.chip->irq_bus_sync_unlock: NULL
	// desc->irq_data.chip->irq_bus_sync_unlock: (kmem_cache#28-oX (irq 347))->irq_data.chip->irq_bus_sync_unlock: NULL
	if (unlikely(desc->irq_data.chip->irq_bus_sync_unlock))
		desc->irq_data.chip->irq_bus_sync_unlock(&desc->irq_data);
}

// ARM10C 20141122
// _IRQ_DESC_CHECK: 1
#define _IRQ_DESC_CHECK		(1 << 0)
#define _IRQ_DESC_PERCPU	(1 << 1)

#define IRQ_GET_DESC_CHECK_GLOBAL	(_IRQ_DESC_CHECK)
#define IRQ_GET_DESC_CHECK_PERCPU	(_IRQ_DESC_CHECK | _IRQ_DESC_PERCPU)

struct irq_desc *
__irq_get_desc_lock(unsigned int irq, unsigned long *flags, bool bus,
		    unsigned int check);
void __irq_put_desc_unlock(struct irq_desc *desc, unsigned long flags, bool bus);

// ARM10C 20141122
// irq: 16, &flags, 0
// ARM10C 20141122
// irq: 32, &flags, 0
// ARM10C 20141213
// irq: 160, &flags, 0
// ARM10C 20141220
// irq: 32, &flags, 0
static inline struct irq_desc *
irq_get_desc_buslock(unsigned int irq, unsigned long *flags, unsigned int check)
{
	// irq: 16, flags: &flags, check: 0
	// __irq_get_desc_lock(16, &flags, true, 0): kmem_cache#28-oX (irq 16)
	return __irq_get_desc_lock(irq, flags, true, check);
	// return kmem_cache#28-oX (irq 16)
}

// ARM10C 20141122
// desc: kmem_cache#28-oX (irq 16)
// ARM10C 20141122
// desc: kmem_cache#28-oX (irq 32)
// ARM10C 20141213
// desc: kmem_cache#28-oX (irq 160)
static inline void
irq_put_desc_busunlock(struct irq_desc *desc, unsigned long flags)
{
	__irq_put_desc_unlock(desc, flags, true);
}

// ARM10C 20141122
// irq: 16, &flags, 0
// ARM10C 20141122
// irq: 32, &flags, 0
// ARM10C 20141213
// irq: 160, &flags, 0
// ARM10C 20141220
// irq: 32, &flags, 0
static inline struct irq_desc *
irq_get_desc_lock(unsigned int irq, unsigned long *flags, unsigned int check)
{
	// irq: 16, flags: &flags, check: 0
	// __irq_get_desc_lock(16, &flags, false, 0): kmem_cache#28-oX (irq 16)
	return __irq_get_desc_lock(irq, flags, false, check);
	// return kmem_cache#28-oX (irq 16)
}

// ARM10C 20141122
// desc: kmem_cache#28-oX (irq 16), flags
// ARM10C 20141122
// desc: kmem_cache#28-oX (irq 32), flags
// ARM10C 20141213
// desc: kmem_cache#28-oX (irq 160), flags
// ARM10C 20141220
// desc: kmem_cache#28-oX (irq 32), flags
static inline void
irq_put_desc_unlock(struct irq_desc *desc, unsigned long flags)
{
	// desc: kmem_cache#28-oX (irq 16), flags
	__irq_put_desc_unlock(desc, flags, false);
}

/*
 * Manipulation functions for irq_data.state
 */
static inline void irqd_set_move_pending(struct irq_data *d)
{
	d->state_use_accessors |= IRQD_SETAFFINITY_PENDING;
}

static inline void irqd_clr_move_pending(struct irq_data *d)
{
	d->state_use_accessors &= ~IRQD_SETAFFINITY_PENDING;
}

// ARM10C 20141122
// &desc->irq_data: &(kmem_cache#28-oX (irq 16))->irq_data, 0xac0f
// ARM10C 20141122
// &desc->irq_data: &(kmem_cache#28-oX (irq 32))->irq_data, 0xac0f
// ARM10C 20141213
// &desc->irq_data: &(kmem_cache#28-oX (irq 160))->irq_data, 0xac0f
// ARM10C 20141220
// &desc->irq_data: &(kmem_cache#28-oX (irq 32))->irq_data, IRQD_IRQ_DISABLED: 0x10000
// ARM10C 20141220
// &desc->irq_data: &(kmem_cache#28-oX (irq 32))->irq_data, IRQD_IRQ_MASKED: 0x20000
// ARM10C 20150509
// &desc->irq_data: &(kmem_cache#28-oX (irq 152))->irq_data, IRQD_IRQ_INPROGRESS: 0x40000
// ARM10C 20150523
// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data, IRQD_IRQ_INPROGRESS: 0x40000
// ARM10C 20150523
// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data, IRQD_IRQ_MASKED: 0x20000
static inline void irqd_clear(struct irq_data *d, unsigned int mask)
{
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10000, mask: 0xac0f
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0x10000, mask: 0xac0f
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 160))->irq_data)->state_use_accessors: 0x10000, mask: 0xac0f
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0x10000, mask: 0x10000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0, mask: 0x20000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x10000, mask: 0x40000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000, mask: 0x40000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000, mask: 0x20000
	d->state_use_accessors &= ~mask;
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 160))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000
}

// ARM10C 20141004
// &desc->irq_data: &(kmem_cache#28-o0)->irq_data, IRQD_IRQ_DISABLED: 0x10000
// ARM10C 20141122
// &desc->irq_data: &(kmem_cache#28-oX (irq 16))->irq_data, IRQD_PER_CPU: 0x800
// ARM10C 20141122
// &desc->irq_data: &(kmem_cache#28-oX (irq 16))->irq_data, irq_settings_get_trigger_mask(kmem_cache#28-oX (irq 16)): 0
// ARM10C 20141122
// &desc->irq_data: &(kmem_cache#28-oX (irq 32))->irq_data, irq_settings_get_trigger_mask(kmem_cache#28-oX (irq 32)): 0
// ARM10C 20141213
// &desc->irq_data: &(kmem_cache#28-oX (irq 160))->irq_data, irq_settings_get_trigger_mask(kmem_cache#28-oX (irq 160)): 0
// ARM10C 20150404
// data: &(kmem_cache#28-oX (irq 152))->irq_data, IRQD_AFFINITY_SET: 0x1000
// ARM10C 20150509
// &desc->irq_data: &(kmem_cache#28-oX (irq 152))->irq_data, IRQD_NO_BALANCING: 0x400
static inline void irqd_set(struct irq_data *d, unsigned int mask)
{
	// d->state_use_accessors: (&(kmem_cache#28-o0)->irq_data)->state_use_accessors: 0, mask: 0x10000
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10000, mask: 0x800
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10800, mask: 0
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0x10000, mask: 0
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 160))->irq_data)->state_use_accessors: 0x10000, mask: 0
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x10000, mask: 0x1000
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11000, mask: 0x400
	d->state_use_accessors |= mask;
	// d->state_use_accessors: (&(kmem_cache#28-o0)->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10800
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 16))->irq_data)->state_use_accessors: 0x10800
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 32))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 160))->irq_data)->state_use_accessors: 0x10000
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11000
	// d->state_use_accessors: (&(kmem_cache#28-oX (irq 152))->irq_data)->state_use_accessors: 0x11400
}

// ARM10C 20150523
// &desc->irq_data: &(kmem_cache#28-oX (irq 347))->irq_data, IRQD_AFFINITY_SET: 0x1000
static inline bool irqd_has_set(struct irq_data *d, unsigned int mask)
{
	// d->state_use_accessors:
	// (&(kmem_cache#28-oX (irq 347))->irq_data)->state_use_accessors: 0x10000, mask: 0x1000
	return d->state_use_accessors & mask;
	// return 0
}
