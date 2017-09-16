/*
 * Generate definitions needed by the preprocessor.
 * This code generates raw asm output which is post-processed
 * to extract and format the required data.
 */

#define __GENERATING_BOUNDS_H
/* Include headers that define the enum constants of interest */
#include <linux/page-flags.h>
#include <linux/mmzone.h>
#include <linux/kbuild.h>
#include <linux/page_cgroup.h>
#include <linux/log2.h>
#include <linux/spinlock_types.h>

void foo(void)
{
	/* The enum constants to put into include/generated/bounds.h */
	// ARM10C 20140405
	DEFINE(NR_PAGEFLAGS, __NR_PAGEFLAGS);
	// ARM10C 20140308
	// ARM10C 20150912
	DEFINE(MAX_NR_ZONES, __MAX_NR_ZONES);
	DEFINE(NR_PCG_FLAGS, __NR_PCG_FLAGS);
#ifdef CONFIG_SMP // CONFIG_SMP=y
	DEFINE(NR_CPUS_BITS, ilog2(CONFIG_NR_CPUS));
#endif
	// ARM10C 20151003
	DEFINE(SPINLOCK_SIZE, sizeof(spinlock_t));
	/* End of constants */
}
