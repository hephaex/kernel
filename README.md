# The Linux Kernel review for ARMv7 3.13.0 (exynos 5420)
All of this repository are written by hephaex@gmail.com.

Community name: IAMROOT.ORG ARM kernel study 10th C team
Target Soc    : Samsung Exynos 5420 (ARMv7 A7&A15 bigLITTLE )
Kernel version: Linux kernel 3.13.x
 - 1st: 3.10.x, A bootstrab before start_kernel(): 3.10.x
 - 2nd: start_kernel()->mm_init: 3.13.x 

# The history of Linux kernel study
* 100th (2015/04/25) week [100차](https://github.com/hephaex/kernel_review/blob/master/a10c_100.md)
 - NIPA 5층 대강당, 12차 Iamroot OT.
 - 개발에 대한 자유토론. (IoT, 3D/4D Printing, 라즈베리파이2, 오드로이드...)
* 99th (2015/04/19) week [99차](https://github.com/hephaex/kernel_review/blob/master/a10c_99.md)
* 98th (2015/04/11) week [98차](https://github.com/hephaex/kernel_review/blob/master/a10c_98.md)
* 97th (2015/04/04) week [97차](https://github.com/hephaex/kernel_review/blob/master/a10c_97.md)
* 96th (2015/03/28) week [96차](https://github.com/hephaex/kernel_review/blob/master/a10c_96.md)
 - start_kernel()->time_init()->clocksource_of_init()->mct_init_spi()->mct_init_dt()->irq_of_parse_and_map()
 - iamroot.org 운영자 회의
* 95th (2015/03/21) week [95차](https://github.com/hephaex/kernel_review/blob/master/a10c_95.md)
 - start_kernel()->time_init()->clocksource_of_init()->mct_init_spi()->mct_init_dt()->irq_of_parse_and_map()->of_irq_parse_one()->of_irq_parse_raw()
* 94th (2015/03/14) week [94차](https://github.com/hephaex/kernel_review/blob/master/a10c_94.md)
 - start_kernel()->time_init()->clocksource_of_init()->mct_init_spi()->mct_init_dt()->irq_of_parse_and_map()
* 93th (2015/03/07) week [93차](https://github.com/hephaex/kernel_review/blob/master/a10c_93.md)
 - start_kernel()->time_init()->of_clk_init()
 - start_kernel()->time_init()->clocksource_of_init()->mct_init_spi()->mct_init_dt()->irq_of_parse_and_map()
* 92th (2015/02/28) week [92차](https://github.com/hephaex/kernel_review/blob/master/a10c_92.md)
 - start_kernel()->time_init()->of_clk_init()
* 91th (2015/02/21) week [91차](https://github.com/hephaex/kernel_review/blob/master/a10c_91.md)
 - start_kernel()->time_init()->of_clk_init()
* 90th (2015/02/07) week [90차](https://github.com/hephaex/kernel_review/blob/master/a10c_90.md)
 - start_kernel()->time_init()->of_clk_init()
* 89th (2015/01/31) week [89차](https://github.com/hephaex/kernel_review/blob/master/a10c_89.md)
 - start_kernel()->time_init()->of_clk_init()
* 88th (2015/01/24) week [88차](https://github.com/hephaex/kernel_review/blob/master/a10c_88.md)
 - start_kernel()->time_init()->of_clk_init()
* 87th (2015/01/17) week [87차](https://github.com/hephaex/kernel_review/blob/master/a10c_87.md)
 - start_kernel()->time_init()->of_clk_init()
* 86th (2015/01/10) week [86차](https://github.com/hephaex/kernel_review/blob/master/a10c_86.md)
 - start_kernel()->time_init()->of_clk_init()
* 85th (2015/01/03) week [85차](https://github.com/hephaex/kernel_review/blob/master/a10c_85.md)
 - start_kernel()->time_init()->of_clk_init()
* 84th (2014/12/27) week [84차](https://github.com/hephaex/kernel_review/blob/master/a10c_84.md)
 - start_kernel()->time_init()->of_clk_init()
* 83th (2014/12/20) week [83차](https://github.com/hephaex/kernel_review/blob/master/a10c_83.md)
* 82th (2014/12/13) week [82차](https://github.com/hephaex/kernel_review/blob/master/a10c_82.md)
* 81th (2014/12/06) week [81차](https://github.com/hephaex/kernel_review/blob/master/a10c_81.md)
 - init_IRQ()->...->kfree()
* 80th (2014/11/29) week [80차](https://github.com/hephaex/kernel_review/blob/master/a10c_80.md)
 - init_IRQ()->...->gic_of_init()->gic_of_bases()
 - irqchip_init()
* 79th (2014/11/22) week [79차](https://github.com/hephaex/kernel_review/blob/master/a10c_79.md)
 - init_IRQ()->...->gic_of_init()->gic_of_bases()
* 78th (2014/11/15) week [78차](https://github.com/hephaex/kernel_review/blob/master/a10c_78.md)
 - init_IRQ()->...->gic_of_init()
 - Radix-tree
* 77th (2014/11/08) week [77차](https://github.com/hephaex/kernel_review/blob/master/a10c_77.md)
 - init_IRQ()->...->gic_of_init()
* 76th (2014/11/01) week [76차](https://github.com/hephaex/kernel_review/blob/master/a10c_76.md)
 - init_IRQ()
* 75th (2014/10/25) week [75차](https://github.com/hephaex/kernel_review/blob/master/a10c_75.md)
 - init_IRQ()
 - RBTree 알고리즘 분석
* 74th (2014/10/18) week [74차](https://github.com/hephaex/kernel_review/blob/master/a10c_74.md)
 - init_IRQ()
* 73th (2014/10/11) week [73차](https://github.com/hephaex/kernel_review/blob/master/a10c_73.md)
 - init_IRQ()
* 72th (2014/10/04) week [72차](https://github.com/hephaex/kernel_review/blob/master/a10c_72.md)
* 71th (2014/09/27) week [71차](https://github.com/hephaex/kernel_review/blob/master/a10c_71.md)
* 70th (2014/09/20) week [70차](https://github.com/hephaex/kernel_review/blob/master/a10c_70.md)
* 69th (2014/09/13) week [69차](https://github.com/hephaex/kernel_review/blob/master/a10c_69.md)
 - sched_init()를 계속 분석
 - sched_init()::for_each_possible_cpu(i) { ... }
 - sched_init()->set_load_weight()
 - sched_init()->plist_head_init()
 - sched_init()->init_idle()
 - sched_init()->zalloc_cpumask_var()
* 68th (2014/08/30) week [68차](https://github.com/hephaex/kernel_review/blob/master/a10c_68.md)
 - sched_init()를 계속 분석
 - sched_init()->init_defrootdomain()
  - // def_root_domain의 맴버 값을 초기화 수행
 - sched_init()->init_rt_bandwidth(&def_rt_bandwidth, global_rt_period(), global_rt_runtime());
 - sched_init()::for_each_possible_cpu(i) { ... } 
* 67th (2014/08/23) week [67차](https://github.com/hephaex/kernel_review/blob/master/a10c_67.md)
 - mm_init() 복습
 - slub()　복습
* 66th (2014/08/16) week [66차](https://github.com/hephaex/kernel_review/blob/master/a10c_66.md)
 - mm_init() 복습;
 - buddy 까지 복습 (mem_init())
* 65th (2014/08/09) week [65차](https://github.com/hephaex/kernel_review/blob/master/a10c_65.md)
 - start_kernel()-> mm_init()-> vmalloc_init();
 - vmlist에 등록된 vm struct 들을 slab으로 이관하고 RB Tree로 구성
* 64th (2014/07/26) week [64차](https://github.com/hephaex/kernel_review/blob/master/a10c_64.md)
 - start_kernel()-> mm_init()-> kmem_cache_init()
 - start_kernel()-> mm_init()-> percpu_init_late()
 - start_kernel()-> mm_init()-> pgtable_cache_init()
* 63th (2014/07/19) week [63차](https://github.com/hephaex/kernel_review/blob/master/a10c_63.md)
 - mm_init()->kmem_cache_init()->bootstrab(&boot_kmem_cache_node) 
* 62th (2014/07/12) week [62차](https://github.com/hephaex/kernel_review/blob/master/a10c_62.md)
 - mm_init()->kmem_cache_init()->bootstrab(&boot_kmem_cache) 
* 61th (2014/07/05) week [61차](https://github.com/hephaex/kernel_review/blob/master/a10c_61.md)
* 60th (2014/06/28) week [60차](https://github.com/hephaex/kernel_review/blob/master/a10c_60.md)
* 59th (2014/06/21) week [59차](https://github.com/hephaex/kernel_review/blob/master/a10c_59.md)
* 58th (2014/06/14) week [58차](https://github.com/hephaex/kernel_review/blob/master/a10c_58.md)
* 57th (2014/06/07) week [57차](https://github.com/hephaex/kernel_review/blob/master/a10c_57.md)
 - start_kernel()->mm_init()->kmem_cache_init()->create_boot_cache()
 - slab_state는 slab이 어느정도 활성화 되었는지를 나타낸다.
 - 지금까지 // slab_state: DOWN 에서 분석을 했고,
 - 이제는 slab_state = PARTIAL; 로 바뀌어 분석을 한다. 
 - 여기서  // slab_state의 의미는  slab을 초기화한 단계를 의미한다.
 - slab_stat = PARTIAL은  kmem_cache_node만 사용이 가능을 의미한다.
 - 계속해서 slab_stat = PARTIAL로 해서 create_boot_cache를 다시 실행한다. 
* 56th (2014/05/31) week [56차](https://github.com/hephaex/kernel_review/blob/master/a10c_56.md)
 - start_kernel()->mm_init()->kmem_cache_init()->create_boot_cache()
 - init_kmem_cache_nodes는 slab으로 사용할 page를 할당받아 설정값(slab_cache, flags, freelist, inuse, frozen)을 바꿔준다.
 - 이후 할당받은 slab object를 kmem_cache_node로 사용하며,
 - kmem_cache_node의 맴버 속성을 초기화합니다. 
 - 초기화되는 맴버속성은 (nr_partial, list_lock, slabs, full)가 있습니다.
* 55th (2014/05/24) week [55차](https://github.com/hephaex/kernel_review/blob/master/a10c_55.md)
 - start_kernel()->mm_init()->kmem_cache_init()->create_boot_cache()
 - new_slab()
* 54th (2014/05/17) week [54차](https://github.com/hephaex/kernel_review/blob/master/a10c_54.md) 
 - buddy할당자에 이어서 kmem 할당자 (Slub)을 분석중입니다. 
 - mem_init()->kmem_cache_init()->create_boot_cache()->__kmem_cache_create()
  ->kmem_cache_open()->init_kmem_cache_nodes->early_kmem_cache_node_alloc()
  ->new_slab()->allocate_slab()->alloc_slab_page()->alloc_pages_exact_node()
  ->__alloc_pages()->__alloc_pages_nodemask();
 - 커널 버전을 3.13.11로 바꿨습니다.
* 53th (2014/05/10) week [53차](https://github.com/hephaex/kernel_review/blob/master/a10c_53.md)  
* 51th (2014/04/19) week [51차](https://github.com/hephaex/kernel_review/blob/master/a10c_51.md)

...

* 23th (2013/09/28) week [23차](https://github.com/hephaex/kernel_review/blob/master/a10c_23.md)
* 22th (2013/09/21) week [22차](https://github.com/hephaex/kernel_review/blob/master/a10c_22.md)
 - boot/compressed/head.S
 - kernel/head.S
* 21th (2013/09/14) week [21차](https://github.com/hephaex/kernel_review/blob/master/a10c_21.md)
 - page_address_init()
 - pr_notice()
 - setup_arch()->setup_processor()
...

* 11th (2012-07-06) week [11차](http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&category=172676&page=6&document_srl=174738) 18+2명
 - arch/arm/boot/compressed/head.S 분석
 - _setup_mmu 종료
* 10th (2012-06-29) week [10차](http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&category=172676&page=6&document_srl=174738) 22명
 - arch/arm/boot/compressed/head.S 분석
 - _setup_mmu 진입직전
* 09th (2012-06-22) week [09차](http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&category=172676&page=6&document_srl=171562) 25명
 - Arm System Developer's Guide (Ch.14 ~ 끝)
 - arch/arm/boot/compressed/head.S 분석
 - call_cache_fn 진입직전
* 08th (2012-06-15) week [08차] 21명
 - Arm System Developer's Guide (Ch.09 ~ Ch.14.4 페이지 테이블)
* 07th (2012-06-08) week [07차] 20명
 - Arm System Developer's Guide (시작 ~ Ch.09 인터럽트 처리방법)
* 06th (2012-06-01) week [06차]
 - ARM v7 아키텍쳐 세미나
* 05th (2012-05-25) week [05차]
 - ARM v7 아키텍쳐 세미나
* 04th (2012-05-18) week [04차] 28명+1 (백창우님)
 - Arm System Developer's Guide (pt자료)
* 03th (2012-05-11) week [03차] 22명
 - 리눅스 커널 내부구조 (p.150 ~ 끝)
* 02th (2012-05-04) week [02차] 27명
 - 리눅스 커널 내부구조 (p. 88~ p.150)
* 01th (2012-04-28) week [01차] 34명
 - 리눅스 커널 내부구조 (처음  ~ p. 88)
