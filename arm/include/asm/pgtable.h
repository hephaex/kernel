/*
 *  arch/arm/include/asm/pgtable.h
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_PGTABLE_H
#define _ASMARM_PGTABLE_H

#include <linux/const.h>
#include <asm/proc-fns.h>

#ifndef CONFIG_MMU

#include <asm-generic/4level-fixup.h>
#include <asm/pgtable-nommu.h>

#else

#include <asm-generic/pgtable-nopud.h>
#include <asm/memory.h>
#include <asm/pgtable-hwdef.h>


#include <asm/tlbflush.h>

#ifdef CONFIG_ARM_LPAE
#include <asm/pgtable-3level.h>
#else
#include <asm/pgtable-2level.h>
#endif

/*
 * Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds memory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 */
// ARM10C 20131019
#define VMALLOC_OFFSET		(8*1024*1024)
// ARM10C 20131102
// ARM10C 20140419
// ARM10C 20141025
// high_memory: 0xef800000, VMALLOC_OFFSET: (8*1024*1024): 0x800000
// VMALLOC_START: 0xf0000000
#define VMALLOC_START		(((unsigned long)high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
// ARM10C 20131019
// ARM10C 20140419
// ARM10C 20140809
// ARM10C 20141025
// VMALLOC_END: 0xff000000UL
#define VMALLOC_END		0xff000000UL

#define LIBRARY_TEXT_START	0x0c000000

#ifndef __ASSEMBLY__
extern void __pte_error(const char *file, int line, pte_t);
extern void __pmd_error(const char *file, int line, pmd_t);
extern void __pgd_error(const char *file, int line, pgd_t);

#define pte_ERROR(pte)		__pte_error(__FILE__, __LINE__, pte)
#define pmd_ERROR(pmd)		__pmd_error(__FILE__, __LINE__, pmd)
#define pgd_ERROR(pgd)		__pgd_error(__FILE__, __LINE__, pgd)

/*
 * This is the lowest virtual address we can permit any user space
 * mapping to be mapped at.  This is particularly important for
 * non-high vector CPUs.
 */
#define FIRST_USER_ADDRESS	(PAGE_SIZE * 2)

/*
 * Use TASK_SIZE as the ceiling argument for free_pgtables() and
 * free_pgd_range() to avoid freeing the modules pmd when LPAE is enabled (pmd
 * page shared between user and kernel).
 */
#ifdef CONFIG_ARM_LPAE
#define USER_PGTABLES_CEILING	TASK_SIZE
#endif

/*
 * The pgprot_* and protection_map entries will be fixed up in runtime
 * to include the cachable and bufferable bits based on memory policy,
 * as well as any architecture dependent bits like global/ASID and SMP
 * shared mapping bits.
 */
#define _L_PTE_DEFAULT	L_PTE_PRESENT | L_PTE_YOUNG

extern pgprot_t		pgprot_user;
extern pgprot_t		pgprot_kernel;
extern pgprot_t		pgprot_hyp_device;
extern pgprot_t		pgprot_s2;
extern pgprot_t		pgprot_s2_device;

// ARM10C 20160813
// pgprot_kernel, L_PTE_XN: 0x200
#define _MOD_PROT(p, b)	__pgprot(pgprot_val(p) | (b))

#define PAGE_NONE		_MOD_PROT(pgprot_user, L_PTE_XN | L_PTE_RDONLY | L_PTE_NONE)
#define PAGE_SHARED		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_XN)
#define PAGE_SHARED_EXEC	_MOD_PROT(pgprot_user, L_PTE_USER)
#define PAGE_COPY		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define PAGE_COPY_EXEC		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY)
#define PAGE_READONLY		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define PAGE_READONLY_EXEC	_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY)
// ARM10C 20160813
// L_PTE_XN: 0x200
#define PAGE_KERNEL		_MOD_PROT(pgprot_kernel, L_PTE_XN)
#define PAGE_KERNEL_EXEC	pgprot_kernel
#define PAGE_HYP		_MOD_PROT(pgprot_kernel, L_PTE_HYP)
#define PAGE_HYP_DEVICE		_MOD_PROT(pgprot_hyp_device, L_PTE_HYP)
#define PAGE_S2			_MOD_PROT(pgprot_s2, L_PTE_S2_RDONLY)
#define PAGE_S2_DEVICE		_MOD_PROT(pgprot_s2_device, L_PTE_S2_RDWR)

// ARM10C 20131026
#define __PAGE_NONE		__pgprot(_L_PTE_DEFAULT | L_PTE_RDONLY | L_PTE_XN | L_PTE_NONE)
#define __PAGE_SHARED		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_XN)
#define __PAGE_SHARED_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER)
#define __PAGE_COPY		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_COPY_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY)
#define __PAGE_READONLY		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_READONLY_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY)

// ARM10C 20160813
#define __pgprot_modify(prot,mask,bits)		\
	__pgprot((pgprot_val(prot) & ~(mask)) | (bits))

#define pgprot_noncached(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_UNCACHED)

#define pgprot_writecombine(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_BUFFERABLE)

#define pgprot_stronglyordered(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_UNCACHED)

#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
#define pgprot_dmacoherent(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_BUFFERABLE | L_PTE_XN)
#define __HAVE_PHYS_MEM_ACCESS_PROT
struct file;
extern pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
				     unsigned long size, pgprot_t vma_prot);
#else
#define pgprot_dmacoherent(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_UNCACHED | L_PTE_XN)
#endif

#endif /* __ASSEMBLY__ */

/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions
 */
// ARM10C 20131026
#define __P000  __PAGE_NONE
#define __P001  __PAGE_READONLY
#define __P010  __PAGE_COPY
#define __P011  __PAGE_COPY
#define __P100  __PAGE_READONLY_EXEC
#define __P101  __PAGE_READONLY_EXEC
#define __P110  __PAGE_COPY_EXEC
#define __P111  __PAGE_COPY_EXEC

#define __S000  __PAGE_NONE
#define __S001  __PAGE_READONLY
#define __S010  __PAGE_SHARED
#define __S011  __PAGE_SHARED
#define __S100  __PAGE_READONLY_EXEC
#define __S101  __PAGE_READONLY_EXEC
#define __S110  __PAGE_SHARED_EXEC
#define __S111  __PAGE_SHARED_EXEC

#ifndef __ASSEMBLY__
/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern struct page *empty_zero_page;
#define ZERO_PAGE(vaddr)	(empty_zero_page)


extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

/* to find an entry in a page-table-directory */
// ARM10C 20131102
// ARM10C 20141025
// ARM10C 20160820
// ARM10C 20160827
// PGDIR_SHIFT:	21
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)

// ARM10C 20131102
// ARM10C 20141025
// ARM10C 20160820
// ARM10C 20160827
// 연산결과 swapper_pg_dir : 0xc0004000
//.pgd = swapper_pg_dir
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))

/* to find an entry in a kernel page-table-directory */
// ARM10C 20131102
// ARM10C 20131109
// ARM10C 20141025
// ARM10C 20160820
// ARM10C 20160827
#define pgd_offset_k(addr)	pgd_offset(&init_mm, addr)

// ARM10C 20131109
// ARM10C 20141101
// ARM10C 20160820
// ARM10C 20160827
#define pmd_none(pmd)		(!pmd_val(pmd))
#define pmd_present(pmd)	(pmd_val(pmd))

// ARM10C 20131123
// pmd: 0x6F7FD8XX
// ARM10C 20141101
// *pmd: *(0xc0004780)
// ARM10C 20160820
// ARM10C 20160827
static inline pte_t *pmd_page_vaddr(pmd_t pmd)
{
	// pmd_val(pmd): 0x6F7FD8XX, PHYS_MASK: 0xFFFFFFFF, PAGE_MASK: 0xFFFFF000
	// __va(pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK): 0xEF7FD000
	return __va(pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK);
}

#define pmd_page(pmd)		pfn_to_page(__phys_to_pfn(pmd_val(pmd) & PHYS_MASK))

#ifndef CONFIG_HIGHPTE
#define __pte_map(pmd)		pmd_page_vaddr(*(pmd))
#define __pte_unmap(pte)	do { } while (0)
#else
#define __pte_map(pmd)		(pte_t *)kmap_atomic(pmd_page(*(pmd)))
#define __pte_unmap(pte)	kunmap_atomic(pte)
#endif

// ARM10C 20131123
// addr: 0xffff0000,  PTRS_PER_PTE: 512
// pte_index(0xffff0000): 0x1F0
// ARM10C 20141101
// addr: 0xf0000000
// pte_index(0xf0000000): 0
// ARM10C 20160820
// ARM10C 20160827
#define pte_index(addr)		(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

// ARM10C 20131123
// pmd: 0xc0007FF8, addr: 0xffff0000
// pmd_page_vaddr(*(pmd)): 0xEF7FD000,  pte_index(0xffff0000): 0x1F0
// pte_offset_kernel(0x4F7FD8XX, 0xffff0000): 0xEF7FD1F0
// ARM10C 20141101
// pmd: 0xc0004780, addr: 0xf0000000
// pmd_page_vaddr(*(0xc0004780)): 0xc0004780이 가리키는 pte의 시작주소, pte_index(0xf0000000): 0
// pte_offset_kernel(0xc0004780,0xf0000000): 0xc0004780이 가리키는 pte의 시작주소
// ARM10C 20160820
// ARM10C 20160827
#define pte_offset_kernel(pmd,addr)	(pmd_page_vaddr(*(pmd)) + pte_index(addr))

#define pte_offset_map(pmd,addr)	(__pte_map(pmd) + pte_index(addr))
#define pte_unmap(pte)			__pte_unmap(pte)

#define pte_pfn(pte)		((pte_val(pte) & PHYS_MASK) >> PAGE_SHIFT)
// ARM10C 20131123
// pfn: 0x6F7FE, __pfn_to_phys(pfn): 0x6F7FE000
// __pte(__pfn_to_phys(pfn) | pgprot_val(prot)): 0x6F7FEXXX
// ARM10C 20141101
// pfn: 0x10481, prot: 0x653
// __pfn_to_phys(0x10481): 0x10481000, pgprot_val(0x653): 0x653
// pfn_pte(0x10481,0x653): 0x10481653
// ARM10C 20160820
#define pfn_pte(pfn,prot)	__pte(__pfn_to_phys(pfn) | pgprot_val(prot))

#define pte_page(pte)		pfn_to_page(pte_pfn(pte))
// ARM10C 20160820
#define mk_pte(page,prot)	pfn_pte(page_to_pfn(page), prot)

// ARM10C 20160827
#define pte_clear(mm,addr,ptep)	set_pte_ext(ptep, __pte(0), 0)

// ARM10C 20141101
// *pte: *(0xc0004780이 가리키는 pte의 시작주소)
// ARM10C 20160820
// ARM10C 20160827
#define pte_none(pte)		(!pte_val(pte))
// ARM10C 20141101
// L_PTE_PRESENT: 0x1
// pte: 0x10481653
// pte_present(0x10481653): 1
// ARM10C 20160827
#define pte_present(pte)	(pte_val(pte) & L_PTE_PRESENT)
#define pte_write(pte)		(!(pte_val(pte) & L_PTE_RDONLY))
#define pte_dirty(pte)		(pte_val(pte) & L_PTE_DIRTY)
#define pte_young(pte)		(pte_val(pte) & L_PTE_YOUNG)
#define pte_exec(pte)		(!(pte_val(pte) & L_PTE_XN))
#define pte_special(pte)	(0)

// ARM10C 20141101
// L_PTE_USER: 0x100
// pteval: 0x10481653
// pte_present(0x10481653): 1
// pte_present_user(0x10481653): 0
#define pte_present_user(pte)  (pte_present(pte) && (pte_val(pte) & L_PTE_USER))

#if __LINUX_ARM_ARCH__ < 6 // __LINUX_ARM_ARCH__: 7
static inline void __sync_icache_dcache(pte_t pteval)
{
}
#else
extern void __sync_icache_dcache(pte_t pteval);
#endif

// ARM10C 20141101
// &init_mm, addr: 0xf0000000, pte: 0xc0004780이 가리키는 pte의 시작주소, pfn_pte(0x10481,0x653): 0x10481653
// ARM10C 20160820
static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pteval)
{
	unsigned long ext = 0;
	// ext: 0

	// addr: 0xf0000000, TASK_SIZE: 0xBF000000, pteval: 0x10481653
	// pte_present_user(0x10481653): 0
	if (addr < TASK_SIZE && pte_present_user(pteval)) {
		__sync_icache_dcache(pteval);
		ext |= PTE_EXT_NG;
	}

	// ptep: 0xc0004780이 가리키는 pte의 시작주소, pteval: 0x10481653, ext: 0
	set_pte_ext(ptep, pteval, ext);

	// set_pte_ext에서 한일:
	// 0xc0004780이 가리키는 pte의 시작주소에 0x10481653 값을 갱신
	// (linux pgtable과 hardware pgtable의 값 같이 갱신)
	//
	//  pgd                   pte
	// |              |
	// +--------------+
	// |              |       +--------------+ +0
	// |              |       |  0xXXXXXXXX  | ---> 0x10481653 에 매칭되는 linux pgtable 값
	// +- - - - - - - +       |  Linux pt 0  |
	// |              |       +--------------+ +1024
	// |              |       |              |
	// +--------------+ +0    |  Linux pt 1  |
	// | *(c0004780)  |-----> +--------------+ +2048
	// |              |       |  0x10481653  | ---> 2052
	// +- - - - - - - + +4    |   h/w pt 0   |
	// | *(c0004784)  |-----> +--------------+ +3072
	// |              |       +              +
	// +--------------+ +8    |   h/w pt 1   |
	// |              |       +--------------+ +4096
}

#define PTE_BIT_FUNC(fn,op) \
static inline pte_t pte_##fn(pte_t pte) { pte_val(pte) op; return pte; }

PTE_BIT_FUNC(wrprotect, |= L_PTE_RDONLY);
PTE_BIT_FUNC(mkwrite,   &= ~L_PTE_RDONLY);
PTE_BIT_FUNC(mkclean,   &= ~L_PTE_DIRTY);
PTE_BIT_FUNC(mkdirty,   |= L_PTE_DIRTY);
PTE_BIT_FUNC(mkold,     &= ~L_PTE_YOUNG);
PTE_BIT_FUNC(mkyoung,   |= L_PTE_YOUNG);

static inline pte_t pte_mkspecial(pte_t pte) { return pte; }

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	const pteval_t mask = L_PTE_XN | L_PTE_RDONLY | L_PTE_USER |
		L_PTE_NONE | L_PTE_VALID;
	pte_val(pte) = (pte_val(pte) & ~mask) | (pgprot_val(newprot) & mask);
	return pte;
}

/*
 * Encode and decode a swap entry.  Swap entries are stored in the Linux
 * page tables as follows:
 *
 *   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 *   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *   <--------------- offset ----------------------> < type -> 0 0 0
 *
 * This gives us up to 31 swap files and 64GB per swap file.  Note that
 * the offset field is always non-zero.
 */
#define __SWP_TYPE_SHIFT	3
#define __SWP_TYPE_BITS		5
#define __SWP_TYPE_MASK		((1 << __SWP_TYPE_BITS) - 1)
#define __SWP_OFFSET_SHIFT	(__SWP_TYPE_BITS + __SWP_TYPE_SHIFT)

#define __swp_type(x)		(((x).val >> __SWP_TYPE_SHIFT) & __SWP_TYPE_MASK)
#define __swp_offset(x)		((x).val >> __SWP_OFFSET_SHIFT)
#define __swp_entry(type,offset) ((swp_entry_t) { ((type) << __SWP_TYPE_SHIFT) | ((offset) << __SWP_OFFSET_SHIFT) })

#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(swp)	((pte_t) { (swp).val })

/*
 * It is an error for the kernel to have more swap files than we can
 * encode in the PTEs.  This ensures that we know when MAX_SWAPFILES
 * is increased beyond what we presently support.
 */
#define MAX_SWAPFILES_CHECK() BUILD_BUG_ON(MAX_SWAPFILES_SHIFT > __SWP_TYPE_BITS)

/*
 * Encode and decode a file entry.  File entries are stored in the Linux
 * page tables as follows:
 *
 *   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 *   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *   <----------------------- offset ------------------------> 1 0 0
 */
#define pte_file(pte)		(pte_val(pte) & L_PTE_FILE)
#define pte_to_pgoff(x)		(pte_val(x) >> 3)
#define pgoff_to_pte(x)		__pte(((x) << 3) | L_PTE_FILE)

#define PTE_FILE_MAX_BITS	29

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
/* FIXME: this is not correct */
#define kern_addr_valid(addr)	(1)

#include <asm-generic/pgtable.h>

/*
 * We provide our own arch_get_unmapped_area to cope with VIPT caches.
 */
#define HAVE_ARCH_UNMAPPED_AREA
#define HAVE_ARCH_UNMAPPED_AREA_TOPDOWN

// ARM10C 20140726
#define pgtable_cache_init() do { } while (0)

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_MMU */

#endif /* _ASMARM_PGTABLE_H */
