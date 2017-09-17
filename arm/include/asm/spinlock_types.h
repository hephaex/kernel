#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H

#ifndef __LINUX_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

#define TICKET_SHIFT	16

// ARM10C 20140125
// ARM10C 20140315
// ARM10C 20140419
// ARM10C 20150620
// sizeof(arch_spinlock_t) : 4 byte
typedef struct {
	union {
		u32 slock;
		struct __raw_tickets {
#ifdef __ARMEB__    // ARM10C N
			u16 next;
			u16 owner;
#else
			u16 owner;  // ARM10C this
			u16 next;
#endif
		} tickets;
	};
} arch_spinlock_t;

// ARM10C 20140315
// ARM10C 20150620
// __ARCH_SPIN_LOCK_UNLOCKED: { { 0 } }
#define __ARCH_SPIN_LOCK_UNLOCKED	{ { 0 } }

typedef struct {
	u32 lock;
} arch_rwlock_t;

// ARM10C 20151031
// __ARCH_RW_LOCK_UNLOCKED: { 0 }
#define __ARCH_RW_LOCK_UNLOCKED		{ 0 }

#endif
