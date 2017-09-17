/*
 * arch/arm/include/asm/pgtable-2level-types.h
 *
 * Copyright (C) 1995-2003 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _ASM_PGTABLE_2LEVEL_TYPES_H
#define _ASM_PGTABLE_2LEVEL_TYPES_H

#include <asm/types.h>

// ARM10C 20131102
// ARM10C 20160820
typedef u32 pteval_t;
// ARM10C 20131026
// ARM10C 20160820
typedef u32 pmdval_t;

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { pteval_t pte; } pte_t;
typedef struct { pmdval_t pmd; } pmd_t;
typedef struct { pmdval_t pgd[2]; } pgd_t;
typedef struct { pteval_t pgprot; } pgprot_t;

#define pte_val(x)      ((x).pte)
#define pmd_val(x)      ((x).pmd)
#define pgd_val(x)	((x).pgd[0])
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x)        ((pte_t) { (x) } )
#define __pmd(x)        ((pmd_t) { (x) } )
#define __pgprot(x)     ((pgprot_t) { (x) } )

#else
/*
 * .. while these make it easier on the compiler
 */
// ARM10C 20131102
// ARM10C 20160820
// ARM10C 20160827
typedef pteval_t pte_t;
// ARM10C 20160820
// ARM10C 20160820
typedef pmdval_t pmd_t;
// ARM10C 20131026
// ARM10C 20150919
// ARM10C 20160820
// ARM10C 20160827
typedef pmdval_t pgd_t[2];
// ARM10C 20131102
typedef pteval_t pgprot_t;

// ARM10C 20141101
// ARM10C 20160820
// ARM10C 20160827
#define pte_val(x)      (x)
// ARM10C 20131109
// ARM10C 20141101
// ARM10C 20160820
#define pmd_val(x)      (x)
// ARM10C 20131026
#define pgd_val(x)	((x)[0])
// ARM10C 20141101
// ARM10C 20160820
#define pgprot_val(x)   (x)

// ARM10C 20131123
// ARM10C 20141101
// ARM10C 20160820
// ARM10C 20160827
#define __pte(x)        (x)
// ARM10C 20131102
#define __pmd(x)        (x)
// ARM10C 20131026
// ARM10C 20131102
// ARM10C 20131123
// ARM10C 20141025
#define __pgprot(x)     (x)

#endif /* STRICT_MM_TYPECHECKS */

#endif	/* _ASM_PGTABLE_2LEVEL_TYPES_H */
