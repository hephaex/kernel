/*
 *  arch/arm/include/asm/cache.h
 */
#ifndef __ASMARM_CACHE_H
#define __ASMARM_CACHE_H

/*
// ARM10C 20130914
// ARM10C 20131207
// CONFIG_ARM_L1_CACHE_SHIFT: 6
// L1_CACHE_SHIFT: 6
*/
#define L1_CACHE_SHIFT		CONFIG_ARM_L1_CACHE_SHIFT
/*
// ARM10C 20131207
// ARM10C 20140419
// ARM10C 20150919
// ARM10C 20151121
// ARM10C 20160319
// ARM10C 20161126
// L1_CACHE_SHIFT: 6
// L1_CACHE_BYTES: 64
*/
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

/*
 * Memory returned by kmalloc() may be used for DMA, so we must make
 * sure that all such allocations are cache aligned. Otherwise,
 * unrelated code may cause parts of the buffer to be read into the
 * cache before the transfer is done, causing old data to be seen by
 * the CPU.
 */
/*
// ARM10C 20140419
// L1_CACHE_BYTES: 64
// ARCH_DMA_MINALIGN: 64
 */
#define ARCH_DMA_MINALIGN	L1_CACHE_BYTES

/*
 * With EABI on ARMv5 and above we must have 64-bit aligned slab pointers.
 */

#if defined(CONFIG_AEABI) && (__LINUX_ARM_ARCH__ >= 5) // CONFIG_AEABI=y, __LINUX_ARM_ARCH__: 7
// ARM10C 20140419
#define ARCH_SLAB_MINALIGN 8
#endif

/*
// ARM10C 20130914
// cache hit 가 자주 될것으로 예상 되는 영역 설정
*/
#define __read_mostly __attribute__((__section__(".data..read_mostly")))

#endif
