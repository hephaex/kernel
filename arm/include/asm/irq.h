#ifndef __ASM_ARM_IRQ_H
#define __ASM_ARM_IRQ_H

// ARM10C 20141004
// NR_IRQS_LEGACY: 16
#define NR_IRQS_LEGACY	16

#ifndef CONFIG_SPARSE_IRQ // CONFIG_SPARSE_IRQ=y
#include <mach/irqs.h>
#else
// ARM10C 20141004
// NR_IRQS_LEGACY: 16
// NR_IRQS: 16
#define NR_IRQS NR_IRQS_LEGACY
#endif

#ifndef irq_canonicalize
#define irq_canonicalize(i)	(i)
#endif

/*
 * Use this value to indicate lack of interrupt
 * capability
 */
#ifndef NO_IRQ
#define NO_IRQ	((unsigned int)(-1))
#endif

#ifndef __ASSEMBLY__
struct irqaction;
struct pt_regs;
extern void migrate_irqs(void);

extern void asm_do_IRQ(unsigned int, struct pt_regs *);
void handle_IRQ(unsigned int, struct pt_regs *);
void init_IRQ(void);

#ifdef CONFIG_MULTI_IRQ_HANDLER // CONFIG_MULTI_IRQ_HANDLER=y
// ARM10C 20141129
extern void (*handle_arch_irq)(struct pt_regs *);
extern void set_handle_irq(void (*handle_irq)(struct pt_regs *));
#endif

#endif

#endif

