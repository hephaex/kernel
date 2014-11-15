#IAMROOT.ORG Kernel스터디10차(ARM)

#HISTORY
* 77th (2014/11/08) week: [77차](https://github.com/hephaex/kernel_review/blob/master/a10c_77.md)
 - init_IRQ()
* 76th (2014/11/01) week: [76차](https://github.com/hephaex/kernel_review/blob/master/a10c_76.md)
 - init_IRQ()
* 75th (2014/10/25) week: [75차](https://github.com/hephaex/kernel_review/blob/master/a10c_75.md)
 - init_IRQ()
 - RBTree 알고리즘
* 74th (2014/10/18) week: [74차](https://github.com/hephaex/kernel_review/blob/master/a10c_74.md)
 - init_IRQ()
* 73th (2014/10/11) week: [73차](https://github.com/hephaex/kernel_review/blob/master/a10c_73.md)
 - init_IRQ()
* 72th (2014/10/04) week: [72차](https://github.com/hephaex/kernel_review/blob/master/a10c_72.md)
 - tick_nohz_init()
 - context_tracking_init()
 - radix_tree_init()
 - early_irq_init()
 - init_IRQ()
* 71th (2014/09/27) week: [71차](https://github.com/hephaex/kernel_review/blob/master/a10c_71.md)
 - rcu_init()
* 70th (2014/09/20) week: [70차](https://github.com/hephaex/kernel_review/blob/master/a10c_70.md)
 - rcu_init()
* 69th (2014/09/13) week: [69차](https://github.com/hephaex/kernel_review/blob/master/a10c_69.md)
 - sched_init()를 계속 분석
 - sched_init()::for_each_possible_cpu(i) { ... }
 - sched_init()->set_load_weight()
 - sched_init()->plist_head_init()
 - sched_init()->init_idle()
 - sched_init()->zalloc_cpumask_var()
* 68th (2014/08/30) week: [68차](https://github.com/hephaex/kernel_review/blob/master/a10c_68.md)
 - sched_init()
 - rq 설정 (for_each_possible_cpu(i))
* 67th (2014/08/23) week: [67차](https://github.com/hephaex/kernel_review/blob/master/a10c_67.md)
 - mm_init() 복습
 - slub() 복습 (kmem_cache_init(), percpu_init_late(), vmalloc_init())
* 66th (2014/08/16) week: [66차](https://github.com/hephaex/kernel_review/blob/master/a10c_66.md)
 - mm_init() 복습;
 - buddy 까지 복습 (mem_init())
* 65th (2014/08/09) week: [65차](https://github.com/hephaex/kernel_review/blob/master/a10c_65.md)
 - start_kernel()-> mm_init()-> vmalloc_init();
 - vmlist에 등록된 vm struct 들을 slab으로 이관하고 RB Tree로 구성
* 64th (2014/07/26) week: [64차](https://github.com/hephaex/kernel_review/blob/master/a10c_64.md)
 - start_kernel()-> mm_init()-> kmem_cache_init()
 - start_kernel()-> mm_init()-> percpu_init_late()
 - start_kernel()-> mm_init()-> pgtable_cache_init()
* 63th (2014/07/19) week: [63차](https://github.com/hephaex/kernel_review/blob/master/a10c_63.md)
 - mm_init()->kmem_cache_init()->bootstrab(&boot_kmem_cache_node) 
* 62th (2014/07/12) week: [62차](https://github.com/hephaex/kernel_review/blob/master/a10c_62.md)
 - mm_init()->kmem_cache_init()->bootstrab(&boot_kmem_cache) 
* 61th (2014/07/05) week: [61차](https://github.com/hephaex/kernel_review/blob/master/a10c_61.md)
* 60th (2014/06/28) week: [60차](https://github.com/hephaex/kernel_review/blob/master/a10c_60.md)
* 59th (2014/06/21) week: [59차](https://github.com/hephaex/kernel_review/blob/master/a10c_59.md)
* 58th (2014/06/14) week: [58차](https://github.com/hephaex/kernel_review/blob/master/a10c_58.md)

...

* 12th (2012-07-13) week: [12차](https://github.com/hephaex/kernel_review/blob/master/a10c_12.md)
 - arch/arm/boot/compressed/head.S
 - restart 진입 후 LC0값 로드
* 11th (2012-07-06) week: [11차](https://github.com/hephaex/kernel_review/blob/master/a10c_11.md)
 - 18+2명
 - arch/arm/boot/compressed/head.S 분석
 - _setup_mmu 종료
* 10th (2012-06-29) week: [10차](https://github.com/hephaex/kernel_review/blob/master/a10c_11.md)
 - 22명
 - arch/arm/boot/compressed/head.S 분석
 - _setup_mmu 진입직전
* 09th (2012-06-22) week: [09차](https://github.com/hephaex/kernel_review/blob/master/a10c_10.md)
 - 25명
 - arch/arm/boot/compressed/head.S 분석
 - call_cache_fn 진입직전
 - Arm System Developer's Guide (Ch.14 ~ 끝)
* 08th (2012-06-15) week: [08차]
 - 21명
 - Arm System Developer's Guide (Ch.09 ~ Ch.14.4 페이지 테이블)
* 07th (2012-06-08) week: [07차]
 - 20명
 - Arm System Developer's Guide (시작 ~ Ch.09 인터럽트 처리방법)
* 06th (2012-06-01) week: [06차]
 - ARM v7 아키텍쳐 세미나
* 05th (2012-05-25) week: [05차]
 - ARM v7 아키텍쳐 세미나
* 04th (2012-05-18) week: [04차]
 - 28명+1 (백창우님)
 - Arm System Developer's Guide (pt자료)
* 03th (2012-05-11) week: [03차]
 - 22명
 - 리눅스 커널 내부구조 (p.150 ~ 끝)
* 02th (2012-05-04) week: [02차]
 - 27명
 - 리눅스 커널 내부구조 (p. 88~ p.150)
* 01th (2012-04-28) week: [01차]
 - 34명
 - 리눅스 커널 내부구조 (처음  ~ p. 88)
=======
# The Linux Kernel review for ARMv7 3.13.0 (exynos 5420)
* Community name: IAMROOT.ORG Linux Kernel 10th "C" team
* Target Soc    : Samsung Exynos 5420 (ARMv7 A7&A15)
* Kernel version: Linux kernel 3.13.x
  - 1st: 3.9.6
  - 2nd: 3.10.x
  - 3th: 3.11.x
  - current : 3.13.x
