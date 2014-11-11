#IAMROOT.ORG Kernel스터디10차(ARM)

#HISTORY
* 73th (2014/10/11) week study : [73차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_73.md)
 - init_IRQ()
* 72th (2014/10/04) week study : [72차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_72.md)
* 71th (2014/09/27) week study : [71차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_71.md)
* 70th (2014/09/20) week study : [70차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_70.md)
* 69th (2014/09/13) week study : [69차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_69.md)
 - sched_init()를 계속 분석
 - sched_init()::for_each_possible_cpu(i) { ... }
 - sched_init()->set_load_weight()
 - sched_init()->plist_head_init()
 - sched_init()->init_idle()
 - sched_init()->zalloc_cpumask_var()
* 68th (2014/08/30) week study : [68차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_68.md)
 - sched_init()를 계속 분석
 - sched_init()->init_defrootdomain()
  - // def_root_domain의 맴버 값을 초기화 수행
 - sched_init()->init_rt_bandwidth(&def_rt_bandwidth, global_rt_period(), global_rt_runtime());
 - sched_init()::for_each_possible_cpu(i) { ... } 
* 67th (2014/08/23) week study : [67차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_67.md)
 - mm_init() 복습
 - slub()　복습
* 66th (2014/08/16) week study : [66차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_66.md)
 - mm_init() 복습;
 - buddy 까지 복습 (mem_init())
* 65th (2014/08/09) week study : [65차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_65.md)
 - start_kernel()-> mm_init()-> vmalloc_init();
 - vmlist에 등록된 vm struct 들을 slab으로 이관하고 RB Tree로 구성
* 64th (2014/07/26) week study : [64차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_64.md)
 - start_kernel()-> mm_init()-> kmem_cache_init()
 - start_kernel()-> mm_init()-> percpu_init_late()
 - start_kernel()-> mm_init()-> pgtable_cache_init()
* 63th (2014/07/19) week study : [63차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_63.md)
 - mm_init()->kmem_cache_init()->bootstrab(&boot_kmem_cache_node) 
* 62th (2014/07/12) week study : [62차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_62.md)
 - mm_init()->kmem_cache_init()->bootstrab(&boot_kmem_cache) 
* 61th (2014/07/05) week study : [61차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_61.md)
* 60th (2014/06/28) week study : [60차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_60.md)
* 59th (2014/06/21) week study : [59차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_59.md)
* 58th (2014/06/14) week study : [58차 분석](https://github.com/hephaex/kernel_review/blob/master/a10c_58.md)

...

* 12th (2012-07-13) week study : [12차 스터디](http://www.iamroot.org/xe/index.php?_filter=search&mid=Kernel_10_ARM&search_keyword=13&search_target=title&page=3&document_srl=176125) 15명
 - arch/arm/boot/compressed/head.S
 - restart 진입 후 LC0값 로드
* 11th (2012-07-06) week study : [11차 스터디](http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&category=172676&page=6&document_srl=174738) 18+2명
 - arch/arm/boot/compressed/head.S 분석
 - _setup_mmu 종료
* 10th (2012-06-29) week study: [10차 스터디](http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&category=172676&page=6&document_srl=174738) 22명
 - arch/arm/boot/compressed/head.S 분석
 - _setup_mmu 진입직전
* 09th (2012-06-22) week study: [09차 스터디](http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&category=172676&page=6&document_srl=171562) 25명
 - arch/arm/boot/compressed/head.S 분석
 - call_cache_fn 진입직전
 - Arm System Developer's Guide (Ch.14 ~ 끝)
* 08th (2012-06-15) week study: [08차 스터디] 21명
 - Arm System Developer's Guide (Ch.09 ~ Ch.14.4 페이지 테이블)
* 07th (2012-06-08) week study: [07차 스터디] 20명
 - Arm System Developer's Guide (시작 ~ Ch.09 인터럽트 처리방법)
* 06th (2012-06-01) week study: [06차 스터디]
 - ARM v7 아키텍쳐 세미나
* 05th (2012-05-25) week study: [05차 스터디]
 - ARM v7 아키텍쳐 세미나
* 04th (2012-05-18) week study: [04차 스터디] 28명+1 (백창우님)
 - Arm System Developer's Guide (pt자료)
* 03th (2012-05-11) week study: [03차 스터디] 22명
 - 리눅스 커널 내부구조 (p.150 ~ 끝)
* 02th (2012-05-04) week study: [02차 스터디] 27명
 - 리눅스 커널 내부구조 (p. 88~ p.150)
* 01th (2012-04-28) week study: [01차 스터디] 34명
 - 리눅스 커널 내부구조 (처음  ~ p. 88)
=======
# The Linux Kernel review for ARMv7 3.13.0 (exynos 5420)
* Community name: IAMROOT.ORG ARM kernel study 10th C team
* Target Soc    : Samsung Exynos 5420 (ARMv7 A7&A15)
* Kernel version: Linux kernel 3.13.x
  - 1st: 3.9.6
  - 2nd: 3.10.x
  - 3th: 3.11.x
  - current : 3.13.x
