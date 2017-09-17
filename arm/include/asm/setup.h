/*
 *  linux/include/asm/setup.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Structure passed to kernel to tell it about the
 *  hardware it's running on.  See Documentation/arm/Setup
 *  for more info.
 */
#ifndef __ASMARM_SETUP_H
#define __ASMARM_SETUP_H

#include <uapi/asm/setup.h>


#define __tag __used __attribute__((__section__(".taglist.init")))
#define __tagtable(tag, fn) \
static const struct tagtable __tagtable_##fn __tag = { tag, fn }

/*
 * Memory map description
 */
// ARM10C 20131012
// CONFIG_ARM_NR_BANKS=8
// ARM은 default가 8임
#define NR_BANKS	CONFIG_ARM_NR_BANKS

// ARM10C 20131012
// ARM10C 20131207
// ARM10C 20140329
struct membank {
	phys_addr_t start;
	phys_addr_t size;
	unsigned int highmem;
};

// ARM10C 20131012
// ARM10C 20131207
// ARM10C 20140329
struct meminfo {
	int nr_banks;
	struct membank bank[NR_BANKS];
};

extern struct meminfo meminfo;

// ARM10C 20131207
// ARM10C 20140329
#define for_each_bank(iter,mi)				\
	for (iter = 0; iter < (mi)->nr_banks; iter++)

// ARM10C 20131019
// ARM10C 20131207
// ARM10C 20140329
#define bank_pfn_start(bank)	__phys_to_pfn((bank)->start)
// ARM10C 20131207
// ARM10C 20140329
#define bank_pfn_end(bank)	__phys_to_pfn((bank)->start + (bank)->size)
#define bank_pfn_size(bank)	((bank)->size >> PAGE_SHIFT)
#define bank_phys_start(bank)	(bank)->start
#define bank_phys_end(bank)	((bank)->start + (bank)->size)
#define bank_phys_size(bank)	(bank)->size

extern int arm_add_memory(u64 start, u64 size);
extern void early_print(const char *str, ...);
extern void dump_machine_table(void);

#endif
