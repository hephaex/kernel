/* CPU control.
 * (C) 2001, 2002, 2003, 2004 Rusty Russell
 *
 * This code is licenced under the GPL.
 */
#include <linux/proc_fs.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/cpu.h>
#include <linux/oom.h>
#include <linux/rcupdate.h>
#include <linux/export.h>
#include <linux/bug.h>
#include <linux/kthread.h>
#include <linux/stop_machine.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/suspend.h>

#include "smpboot.h"

#ifdef CONFIG_SMP // CONFIG_SMP=y
/* Serializes the updates to cpu_online_mask, cpu_present_mask */
// ARM10C 20140315
// DEFINE_MUTEX(cpu_add_remove_lock):
// struct mutex cpu_add_remove_lock =
// { .count = { (1) }
//    , .wait_lock =
//    (spinlock_t )
//    { { .rlock =
//	  {
//	  .raw_lock = { { 0 } },
//	  .magic = 0xdead4ead,
//	  .owner_cpu = -1,
//	  .owner = 0xffffffff,
//	  }
//    } }
//    , .wait_list =
//    { &(cpu_add_remove_lock.wait_list), &(cpu_add_remove_lock.wait_list) }
//    , .magic = &cpu_add_remove_lock
// }
static DEFINE_MUTEX(cpu_add_remove_lock);

/*
 * The following two API's must be used when attempting
 * to serialize the updates to cpu_online_mask, cpu_present_mask.
 */
// ARM10C 20140315
void cpu_maps_update_begin(void)
{
	mutex_lock(&cpu_add_remove_lock);
}

// ARM10C 20140322
void cpu_maps_update_done(void)
{
	mutex_unlock(&cpu_add_remove_lock);
}

// ARM10C 20140322
// RAW_NOTIFIER_HEAD(cpu_chain):
// struct raw_notifier_head cpu_chain = { .head = NULL }
// ARM10C 20140726
static RAW_NOTIFIER_HEAD(cpu_chain);

/* If set, cpu_up and cpu_down will return -EBUSY and do nothing.
 * Should always be manipulated under cpu_add_remove_lock
 */
static int cpu_hotplug_disabled;

#ifdef CONFIG_HOTPLUG_CPU

// ARM10C 20140920
static struct {
	struct task_struct *active_writer;
	struct mutex lock; /* Synchronizes accesses to refcount, */
	/*
	 * Also blocks the new readers during
	 * an ongoing cpu hotplug operation.
	 */
	int refcount;
} cpu_hotplug = {
	.active_writer = NULL,
	.lock = __MUTEX_INITIALIZER(cpu_hotplug.lock),
	.refcount = 0,
};

// ARM10C 20140920
void get_online_cpus(void)
{
	might_sleep(); // null function

	// cpu_hotplug.active_writer: NULL, current: &init_task
	if (cpu_hotplug.active_writer == current)
		return;

	mutex_lock(&cpu_hotplug.lock);
	// &cpu_hotplug.lock을 사용한 mutex lock 수행

	// cpu_hotplug.refcount: 0
	cpu_hotplug.refcount++;
	// cpu_hotplug.refcount: 1

	mutex_unlock(&cpu_hotplug.lock);
	// &cpu_hotplug.lock을 사용한 mutex lock 해재

}
EXPORT_SYMBOL_GPL(get_online_cpus);

// ARM10C 20140920
void put_online_cpus(void)
{
	// cpu_hotplug.active_writer: NULL, current: &init_task
	if (cpu_hotplug.active_writer == current)
		return;

	mutex_lock(&cpu_hotplug.lock);
	// &cpu_hotplug.lock을 사용한 mutex lock 수행

	// cpu_hotplug.refcount: 1
	if (WARN_ON(!cpu_hotplug.refcount))
		cpu_hotplug.refcount++; /* try to fix things up */

	// cpu_hotplug.refcount: 1, cpu_hotplug.active_writer: NULL
	if (!--cpu_hotplug.refcount && unlikely(cpu_hotplug.active_writer))
		wake_up_process(cpu_hotplug.active_writer);
	// cpu_hotplug.refcount: 0

	mutex_unlock(&cpu_hotplug.lock);
	// &cpu_hotplug.lock을 사용한 mutex lock 해재

}
EXPORT_SYMBOL_GPL(put_online_cpus);

/*
 * This ensures that the hotplug operation can begin only when the
 * refcount goes to zero.
 *
 * Note that during a cpu-hotplug operation, the new readers, if any,
 * will be blocked by the cpu_hotplug.lock
 *
 * Since cpu_hotplug_begin() is always called after invoking
 * cpu_maps_update_begin(), we can be sure that only one writer is active.
 *
 * Note that theoretically, there is a possibility of a livelock:
 * - Refcount goes to zero, last reader wakes up the sleeping
 *   writer.
 * - Last reader unlocks the cpu_hotplug.lock.
 * - A new reader arrives at this moment, bumps up the refcount.
 * - The writer acquires the cpu_hotplug.lock finds the refcount
 *   non zero and goes to sleep again.
 *
 * However, this is very difficult to achieve in practice since
 * get_online_cpus() not an api which is called all that often.
 *
 */
void cpu_hotplug_begin(void)
{
	cpu_hotplug.active_writer = current;

	for (;;) {
		mutex_lock(&cpu_hotplug.lock);
		if (likely(!cpu_hotplug.refcount))
			break;
		__set_current_state(TASK_UNINTERRUPTIBLE);
		mutex_unlock(&cpu_hotplug.lock);
		schedule();
	}
}

void cpu_hotplug_done(void)
{
	cpu_hotplug.active_writer = NULL;
	mutex_unlock(&cpu_hotplug.lock);
}

/*
 * Wait for currently running CPU hotplug operations to complete (if any) and
 * disable future CPU hotplug (from sysfs). The 'cpu_add_remove_lock' protects
 * the 'cpu_hotplug_disabled' flag. The same lock is also acquired by the
 * hotplug path before performing hotplug operations. So acquiring that lock
 * guarantees mutual exclusion from any currently running hotplug operations.
 */
void cpu_hotplug_disable(void)
{
	cpu_maps_update_begin();
	cpu_hotplug_disabled = 1;
	cpu_maps_update_done();
}

void cpu_hotplug_enable(void)
{
	cpu_maps_update_begin();
	cpu_hotplug_disabled = 0;
	cpu_maps_update_done();
}

#endif	/* CONFIG_HOTPLUG_CPU */

/* Need to know about CPUs going up/down? */
// ARM10C 20140315
// nb: &page_alloc_cpu_notify_nb
// ARM10C 20140726
// &slab_notifier
// ARM10C 20140920
// &sched_ilb_notifier_nb
// ARM10C 20140927
// &rcu_cpu_notify_nb
// ARM10C 20141004
// &radix_tree_callback_nb
// ARM10C 20141129
// &gic_cpu_notifier
// ARM10C 20150103
// &timers_nb
// ARM10C 20150103
// &hrtimers_nb
// ARM10C 20150404
// &exynos4_mct_cpu_nb
// ARM10C 20150620
// &hotplug_cfd_notifier
// ARM10C 20151003
// &buffer_cpu_notify_nb
// ARM10C 20160604
// &ratelimit_nb
int __ref register_cpu_notifier(struct notifier_block *nb)
{
	int ret;
	cpu_maps_update_begin();
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.
	// waiter를 만들어 mutex를 lock을 시도하며 기다리다 가능할 때 mutex lock한다.

	// &cpu_chain, nb: &page_alloc_cpu_notify_nb
	// raw_notifier_chain_register(&cpu_chain, &page_alloc_cpu_notify_nb): 0
	// &cpu_chain, nb: &slab_notifier
	// raw_notifier_chain_register(&cpu_chain, &slab_notifier): 0
	// &cpu_chain, nb: &sched_ilb_notifier_nb
	// raw_notifier_chain_register(&cpu_chain, &sched_ilb_notifier_nb): 0
	// &cpu_chain, nb: &rcu_cpu_notify_nb
	// raw_notifier_chain_register(&cpu_chain, &rcu_cpu_notify_nb): 0
	// &cpu_chain, nb: &radix_tree_callback_nb
	// raw_notifier_chain_register(&cpu_chain, &radix_tree_callback_nb): 0
	// &cpu_chain, nb: &gic_cpu_notifier
	// raw_notifier_chain_register(&cpu_chain, &gic_cpu_notifier): 0
	// &cpu_chain, nb: &timers_nb
	// raw_notifier_chain_register(&cpu_chain, &timers_nb): 0
	// &cpu_chain, nb: &hrtimers_nb
	// raw_notifier_chain_register(&cpu_chain, &hrtimers_nb): 0
	// &cpu_chain, nb: &exynos4_mct_cpu_nb
	// raw_notifier_chain_register(&cpu_chain, &exynos4_mct_cpu_nb): 0
	// &cpu_chain, nb: &hotplug_cfd_notifier
	// raw_notifier_chain_register(&cpu_chain, &hotplug_cfd_notifier): 0
	// &cpu_chain, nb: &buffer_cpu_notify_nb
	// raw_notifier_chain_register(&cpu_chain, &buffer_cpu_notify_nb): 0
	// &cpu_chain, nb: &ratelimit_nb
	// raw_notifier_chain_register(&cpu_chain, &ratelimit_nb): 0
	ret = raw_notifier_chain_register(&cpu_chain, nb);
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0

	// raw_notifier_chain_register(&page_alloc_cpu_notify_nb) 에서 한일:
	//
	// (&cpu_chain)->head: page_alloc_cpu_notify_nb 포인터 대입
	// (&page_alloc_cpu_notify_nb)->next은 NULL로 대입

	// raw_notifier_chain_register(&slab_notifier) 에서 한일:
	//
	// (&cpu_chain)->head: slab_notifier 포인터 대입
	// (&slab_notifier)->next은 (&page_alloc_cpu_notify_nb)->next로 대입

	// raw_notifier_chain_register(&sched_ilb_notifier_nb) 에서 한일:
	//
	// (&cpu_chain)->head: sched_ilb_notifier_nb 포인터 대입
	// (&sched_ilb_notifier_nb)->next은 (&slab_notifier)->next로 대입

	// raw_notifier_chain_register(&rcu_cpu_notify_nb) 에서 한일:
	//
	// (&cpu_chain)->head: rcu_cpu_notify_nb 포인터 대입
	// (&rcu_cpu_notify_nb)->next은 (&sched_ilb_notifier_nb)->next로 대입

	// raw_notifier_chain_register(&radix_tree_callback_nb) 에서 한일:
	//
	// (&cpu_chain)->head: radix_tree_callback_nb 포인터 대입
	// (&radix_tree_callback_nb)->next은 (&rcu_cpu_notify_nb)->next로 대입

	// raw_notifier_chain_register(&gic_cpu_notifier) 에서 한일:
	//
	// (&cpu_chain)->head: gic_cpu_notifier 포인터 대입
	// (&gic_cpu_notifier)->next은 (&radix_tree_callback_nb)->next로 대입

	// raw_notifier_chain_register(&timers_nb) 에서 한일:
	//
	// (&cpu_chain)->head: timers_nb 포인터 대입
	// (&timers_nb)->next은 (&gic_cpu_notifier)->next로 대입

	// raw_notifier_chain_register(&hrtimers_nb) 에서 한일:
	//
	// (&cpu_chain)->head: &hrtimers_nb 포인터 대입
	// (&hrtimers_nb)->next은 (&timers_nb)->next로 대입

	// raw_notifier_chain_register(&exynos4_mct_cpu_nb) 에서 한일:
	//
	// (&cpu_chain)->head: &exynos4_mct_cpu_nb 포인터 대입
	// (&exynos4_mct_cpu_nb)->next은 (&hrtimers_nb)->next로 대입

	// raw_notifier_chain_register(&hotplug_cfd_notifier) 에서 한일:
	//
	// (&cpu_chain)->head: &hotplug_cfd_notifier
	// (&hotplug_cfd_notifier)->next은 (&exynos4_mct_cpu_nb)->next로 대입

	// raw_notifier_chain_register(&buffer_cpu_notify_nb) 에서 한일:
	//
	// (&cpu_chain)->head: &buffer_cpu_notify_nb
	// (&buffer_cpu_notify_nb)->next은 (&hotplug_cfd_notifier)->next로 대입

	// raw_notifier_chain_register(&ratelimit_nb) 에서 한일:
	//
	// (&cpu_chain)->head: &ratelimit_nb
	// (&ratelimit_nb)->next은 (&buffer_cpu_notify_nb)->next로 대입

	cpu_maps_update_done();
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.
	// mutex를 기다리는(waiter)가 있으면 깨우고 아니면 mutex unlock한다.

	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	// ret: 0
	return ret;
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
	// return 0
}

static int __cpu_notify(unsigned long val, void *v, int nr_to_call,
			int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&cpu_chain, val, v, nr_to_call,
					nr_calls);

	return notifier_to_errno(ret);
}

static int cpu_notify(unsigned long val, void *v)
{
	return __cpu_notify(val, v, -1, NULL);
}

#ifdef CONFIG_HOTPLUG_CPU

static void cpu_notify_nofail(unsigned long val, void *v)
{
	BUG_ON(cpu_notify(val, v));
}
EXPORT_SYMBOL(register_cpu_notifier);

void __ref unregister_cpu_notifier(struct notifier_block *nb)
{
	cpu_maps_update_begin();
	raw_notifier_chain_unregister(&cpu_chain, nb);
	cpu_maps_update_done();
}
EXPORT_SYMBOL(unregister_cpu_notifier);

/**
 * clear_tasks_mm_cpumask - Safely clear tasks' mm_cpumask for a CPU
 * @cpu: a CPU id
 *
 * This function walks all processes, finds a valid mm struct for each one and
 * then clears a corresponding bit in mm's cpumask.  While this all sounds
 * trivial, there are various non-obvious corner cases, which this function
 * tries to solve in a safe manner.
 *
 * Also note that the function uses a somewhat relaxed locking scheme, so it may
 * be called only for an already offlined CPU.
 */
void clear_tasks_mm_cpumask(int cpu)
{
	struct task_struct *p;

	/*
	 * This function is called after the cpu is taken down and marked
	 * offline, so its not like new tasks will ever get this cpu set in
	 * their mm mask. -- Peter Zijlstra
	 * Thus, we may use rcu_read_lock() here, instead of grabbing
	 * full-fledged tasklist_lock.
	 */
	WARN_ON(cpu_online(cpu));
	rcu_read_lock();
	for_each_process(p) {
		struct task_struct *t;

		/*
		 * Main thread might exit, but other threads may still have
		 * a valid mm. Find one.
		 */
		t = find_lock_task_mm(p);
		if (!t)
			continue;
		cpumask_clear_cpu(cpu, mm_cpumask(t->mm));
		task_unlock(t);
	}
	rcu_read_unlock();
}

static inline void check_for_tasks(int cpu)
{
	struct task_struct *p;
	cputime_t utime, stime;

	write_lock_irq(&tasklist_lock);
	for_each_process(p) {
		task_cputime(p, &utime, &stime);
		if (task_cpu(p) == cpu && p->state == TASK_RUNNING &&
		    (utime || stime))
			printk(KERN_WARNING "Task %s (pid = %d) is on cpu %d "
				"(state = %ld, flags = %x)\n",
				p->comm, task_pid_nr(p), cpu,
				p->state, p->flags);
	}
	write_unlock_irq(&tasklist_lock);
}

struct take_cpu_down_param {
	unsigned long mod;
	void *hcpu;
};

/* Take this CPU down. */
static int __ref take_cpu_down(void *_param)
{
	struct take_cpu_down_param *param = _param;
	int err;

	/* Ensure this CPU doesn't handle any more interrupts. */
	err = __cpu_disable();
	if (err < 0)
		return err;

	cpu_notify(CPU_DYING | param->mod, param->hcpu);
	/* Park the stopper thread */
	kthread_park(current);
	return 0;
}

/* Requires cpu_add_remove_lock to be held */
static int __ref _cpu_down(unsigned int cpu, int tasks_frozen)
{
	int err, nr_calls = 0;
	void *hcpu = (void *)(long)cpu;
	unsigned long mod = tasks_frozen ? CPU_TASKS_FROZEN : 0;
	struct take_cpu_down_param tcd_param = {
		.mod = mod,
		.hcpu = hcpu,
	};

	if (num_online_cpus() == 1)
		return -EBUSY;

	if (!cpu_online(cpu))
		return -EINVAL;

	cpu_hotplug_begin();

	err = __cpu_notify(CPU_DOWN_PREPARE | mod, hcpu, -1, &nr_calls);
	if (err) {
		nr_calls--;
		__cpu_notify(CPU_DOWN_FAILED | mod, hcpu, nr_calls, NULL);
		printk("%s: attempt to take down CPU %u failed\n",
				__func__, cpu);
		goto out_release;
	}

	/*
	 * By now we've cleared cpu_active_mask, wait for all preempt-disabled
	 * and RCU users of this state to go away such that all new such users
	 * will observe it.
	 *
	 * For CONFIG_PREEMPT we have preemptible RCU and its sync_rcu() might
	 * not imply sync_sched(), so explicitly call both.
	 *
	 * Do sync before park smpboot threads to take care the rcu boost case.
	 */
#ifdef CONFIG_PREEMPT
	synchronize_sched();
#endif
	synchronize_rcu();

	smpboot_park_threads(cpu);

	/*
	 * So now all preempt/rcu users must observe !cpu_active().
	 */

	err = __stop_machine(take_cpu_down, &tcd_param, cpumask_of(cpu));
	if (err) {
		/* CPU didn't die: tell everyone.  Can't complain. */
		smpboot_unpark_threads(cpu);
		cpu_notify_nofail(CPU_DOWN_FAILED | mod, hcpu);
		goto out_release;
	}
	BUG_ON(cpu_online(cpu));

	/*
	 * The migration_call() CPU_DYING callback will have removed all
	 * runnable tasks from the cpu, there's only the idle task left now
	 * that the migration thread is done doing the stop_machine thing.
	 *
	 * Wait for the stop thread to go away.
	 */
	while (!idle_cpu(cpu))
		cpu_relax();

	/* This actually kills the CPU. */
	__cpu_die(cpu);

	/* CPU is completely dead: tell everyone.  Too late to complain. */
	cpu_notify_nofail(CPU_DEAD | mod, hcpu);

	check_for_tasks(cpu);

out_release:
	cpu_hotplug_done();
	if (!err)
		cpu_notify_nofail(CPU_POST_DEAD | mod, hcpu);
	return err;
}

int __ref cpu_down(unsigned int cpu)
{
	int err;

	cpu_maps_update_begin();

	if (cpu_hotplug_disabled) {
		err = -EBUSY;
		goto out;
	}

	err = _cpu_down(cpu, 0);

out:
	cpu_maps_update_done();
	return err;
}
EXPORT_SYMBOL(cpu_down);
#endif /*CONFIG_HOTPLUG_CPU*/

/* Requires cpu_add_remove_lock to be held */
static int _cpu_up(unsigned int cpu, int tasks_frozen)
{
	int ret, nr_calls = 0;
	void *hcpu = (void *)(long)cpu;
	unsigned long mod = tasks_frozen ? CPU_TASKS_FROZEN : 0;
	struct task_struct *idle;

	cpu_hotplug_begin();

	if (cpu_online(cpu) || !cpu_present(cpu)) {
		ret = -EINVAL;
		goto out;
	}

	idle = idle_thread_get(cpu);
	if (IS_ERR(idle)) {
		ret = PTR_ERR(idle);
		goto out;
	}

	ret = smpboot_create_threads(cpu);
	if (ret)
		goto out;

	ret = __cpu_notify(CPU_UP_PREPARE | mod, hcpu, -1, &nr_calls);
	if (ret) {
		nr_calls--;
		printk(KERN_WARNING "%s: attempt to bring up CPU %u failed\n",
				__func__, cpu);
		goto out_notify;
	}

	/* Arch-specific enabling code. */
	ret = __cpu_up(cpu, idle);
	if (ret != 0)
		goto out_notify;
	BUG_ON(!cpu_online(cpu));

	/* Wake the per cpu threads */
	smpboot_unpark_threads(cpu);

	/* Now call notifier in preparation. */
	cpu_notify(CPU_ONLINE | mod, hcpu);

out_notify:
	if (ret != 0)
		__cpu_notify(CPU_UP_CANCELED | mod, hcpu, nr_calls, NULL);
out:
	cpu_hotplug_done();

	return ret;
}

int cpu_up(unsigned int cpu)
{
	int err = 0;

	if (!cpu_possible(cpu)) {
		printk(KERN_ERR "can't online cpu %d because it is not "
			"configured as may-hotadd at boot time\n", cpu);
#if defined(CONFIG_IA64)
		printk(KERN_ERR "please check additional_cpus= boot "
				"parameter\n");
#endif
		return -EINVAL;
	}

	err = try_online_node(cpu_to_node(cpu));
	if (err)
		return err;

	cpu_maps_update_begin();

	if (cpu_hotplug_disabled) {
		err = -EBUSY;
		goto out;
	}

	err = _cpu_up(cpu, 0);

out:
	cpu_maps_update_done();
	return err;
}
EXPORT_SYMBOL_GPL(cpu_up);

#ifdef CONFIG_PM_SLEEP_SMP
static cpumask_var_t frozen_cpus;

int disable_nonboot_cpus(void)
{
	int cpu, first_cpu, error = 0;

	cpu_maps_update_begin();
	first_cpu = cpumask_first(cpu_online_mask);
	/*
	 * We take down all of the non-boot CPUs in one shot to avoid races
	 * with the userspace trying to use the CPU hotplug at the same time
	 */
	cpumask_clear(frozen_cpus);

	printk("Disabling non-boot CPUs ...\n");
	for_each_online_cpu(cpu) {
		if (cpu == first_cpu)
			continue;
		error = _cpu_down(cpu, 1);
		if (!error)
			cpumask_set_cpu(cpu, frozen_cpus);
		else {
			printk(KERN_ERR "Error taking CPU%d down: %d\n",
				cpu, error);
			break;
		}
	}

	if (!error) {
		BUG_ON(num_online_cpus() > 1);
		/* Make sure the CPUs won't be enabled by someone else */
		cpu_hotplug_disabled = 1;
	} else {
		printk(KERN_ERR "Non-boot CPUs are not disabled\n");
	}
	cpu_maps_update_done();
	return error;
}

void __weak arch_enable_nonboot_cpus_begin(void)
{
}

void __weak arch_enable_nonboot_cpus_end(void)
{
}

void __ref enable_nonboot_cpus(void)
{
	int cpu, error;

	/* Allow everyone to use the CPU hotplug again */
	cpu_maps_update_begin();
	cpu_hotplug_disabled = 0;
	if (cpumask_empty(frozen_cpus))
		goto out;

	printk(KERN_INFO "Enabling non-boot CPUs ...\n");

	arch_enable_nonboot_cpus_begin();

	for_each_cpu(cpu, frozen_cpus) {
		error = _cpu_up(cpu, 1);
		if (!error) {
			printk(KERN_INFO "CPU%d is up\n", cpu);
			continue;
		}
		printk(KERN_WARNING "Error taking CPU%d up: %d\n", cpu, error);
	}

	arch_enable_nonboot_cpus_end();

	cpumask_clear(frozen_cpus);
out:
	cpu_maps_update_done();
}

static int __init alloc_frozen_cpus(void)
{
	if (!alloc_cpumask_var(&frozen_cpus, GFP_KERNEL|__GFP_ZERO))
		return -ENOMEM;
	return 0;
}
core_initcall(alloc_frozen_cpus);

/*
 * When callbacks for CPU hotplug notifications are being executed, we must
 * ensure that the state of the system with respect to the tasks being frozen
 * or not, as reported by the notification, remains unchanged *throughout the
 * duration* of the execution of the callbacks.
 * Hence we need to prevent the freezer from racing with regular CPU hotplug.
 *
 * This synchronization is implemented by mutually excluding regular CPU
 * hotplug and Suspend/Hibernate call paths by hooking onto the Suspend/
 * Hibernate notifications.
 */
static int
cpu_hotplug_pm_callback(struct notifier_block *nb,
			unsigned long action, void *ptr)
{
	switch (action) {

	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		cpu_hotplug_disable();
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		cpu_hotplug_enable();
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}


static int __init cpu_hotplug_pm_sync_init(void)
{
	/*
	 * cpu_hotplug_pm_callback has higher priority than x86
	 * bsp_pm_callback which depends on cpu_hotplug_pm_callback
	 * to disable cpu hotplug to avoid cpu hotplug race.
	 */
	pm_notifier(cpu_hotplug_pm_callback, 0);
	return 0;
}
core_initcall(cpu_hotplug_pm_sync_init);

#endif /* CONFIG_PM_SLEEP_SMP */

/**
 * notify_cpu_starting(cpu) - call the CPU_STARTING notifiers
 * @cpu: cpu that just started
 *
 * This function calls the cpu_chain notifiers with CPU_STARTING.
 * It must be called by the arch code on the new cpu, before the new cpu
 * enables interrupts and before the "boot" cpu returns from __cpu_up().
 */
void notify_cpu_starting(unsigned int cpu)
{
	unsigned long val = CPU_STARTING;

#ifdef CONFIG_PM_SLEEP_SMP
	if (frozen_cpus != NULL && cpumask_test_cpu(cpu, frozen_cpus))
		val = CPU_STARTING_FROZEN;
#endif /* CONFIG_PM_SLEEP_SMP */
	cpu_notify(val, (void *)(long)cpu);
}

#endif /* CONFIG_SMP */

/*
 * cpu_bit_bitmap[] is a special, "compressed" data structure that
 * represents all NR_CPUS bits binary values of 1<<nr.
 *
 * It is used by cpumask_of() to get a constant address to a CPU
 * mask value that has a single bit set only.
 */

/* cpu_bit_bitmap[0] is empty - so we can back into it */
#define MASK_DECLARE_1(x)	[x+1][0] = (1UL << (x))
#define MASK_DECLARE_2(x)	MASK_DECLARE_1(x), MASK_DECLARE_1(x+1)
#define MASK_DECLARE_4(x)	MASK_DECLARE_2(x), MASK_DECLARE_2(x+2)
#define MASK_DECLARE_8(x)	MASK_DECLARE_4(x), MASK_DECLARE_4(x+4)

// ARM10C 20130831
// ARM10C 20140913
// cpu_bit_bitmap[33][1]
const unsigned long cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(NR_CPUS)] = {
//      MASK_DECLARE_8(0) 이 아래와 같이 확장됨
//	[1][0] = (1UL << 0),
//	[2][0] = (1UL << 1),
//	[3][0] = (1UL << 2),
//	[4][0] = (1UL << 3),
//	[5][0] = (1UL << 4),
//	[6][0] = (1UL << 5),
//	[7][0] = (1UL << 6),

	MASK_DECLARE_8(0),	MASK_DECLARE_8(8),
	MASK_DECLARE_8(16),	MASK_DECLARE_8(24),
#if BITS_PER_LONG > 32
	MASK_DECLARE_8(32),	MASK_DECLARE_8(40),
	MASK_DECLARE_8(48),	MASK_DECLARE_8(56),
#endif
};
EXPORT_SYMBOL_GPL(cpu_bit_bitmap);

const DECLARE_BITMAP(cpu_all_bits, NR_CPUS) = CPU_BITS_ALL;
EXPORT_SYMBOL(cpu_all_bits);

#ifdef CONFIG_INIT_ALL_POSSIBLE // CONFIG_INIT_ALL_POSSIBLE=n
static DECLARE_BITMAP(cpu_possible_bits, CONFIG_NR_CPUS) __read_mostly
	= CPU_BITS_ALL;
#else
// ARM10C 20140215
// ARM10C 20140607
// CONFIG_NR_CPUS: 4
// cpu_possible_bits, 4
static DECLARE_BITMAP(cpu_possible_bits, CONFIG_NR_CPUS) __read_mostly;
#endif
// ARM10C 20140215
// ARM10C 20140301
// ARM10C 20140607
// cpu_possible_mask: cpu_possible_bits
const struct cpumask *const cpu_possible_mask = to_cpumask(cpu_possible_bits);
EXPORT_SYMBOL(cpu_possible_mask);

// ARM10C 20140927
// ARM10C 20150328
// ARM10C 20150523
// ARM10C 20160604
// ARM10C 20170201
// CONFIG_NR_CPUS: 4
static DECLARE_BITMAP(cpu_online_bits, CONFIG_NR_CPUS) __read_mostly;
// ARM10C 20140927
// ARM10C 20150328
// ARM10C 20150523
// ARM10C 20160604
// ARM10C 20170201
const struct cpumask *const cpu_online_mask = to_cpumask(cpu_online_bits);
EXPORT_SYMBOL(cpu_online_mask);

static DECLARE_BITMAP(cpu_present_bits, CONFIG_NR_CPUS) __read_mostly;
const struct cpumask *const cpu_present_mask = to_cpumask(cpu_present_bits);
EXPORT_SYMBOL(cpu_present_mask);

// ARM10C 20140913
// CONFIG_NR_CPUS: 4
// cpu_active_bits[1]
static DECLARE_BITMAP(cpu_active_bits, CONFIG_NR_CPUS) __read_mostly;

// ARM10C 20140913
// to_cpumask(cpu_active_bits): cpu_active_bits[1]
// cpu_active_mask: cpu_active_bits[1]
const struct cpumask *const cpu_active_mask = to_cpumask(cpu_active_bits);
EXPORT_SYMBOL(cpu_active_mask);

// ARM10C 20130907 cpu = 0, passible = 1
// cpu_possible_bits[ 0 ] = 1 이됨
// ARM10C 20140215
// i: 0, true
void set_cpu_possible(unsigned int cpu, bool possible)
{
	if (possible)
		cpumask_set_cpu(cpu, to_cpumask(cpu_possible_bits));
	else
		cpumask_clear_cpu(cpu, to_cpumask(cpu_possible_bits));
}

// ARM10C 20130907 cpu = 0, present = 1
// cpu_present_bits[ 0 ] = 1 이됨
void set_cpu_present(unsigned int cpu, bool present)
{
	if (present)
		cpumask_set_cpu(cpu, to_cpumask(cpu_present_bits));
	else
		cpumask_clear_cpu(cpu, to_cpumask(cpu_present_bits));
}

// ARM10C 20130907 cpu = 0, online = 1
// cpu_online_bits[ 0 ] = 1 이됨
void set_cpu_online(unsigned int cpu, bool online)
{
	if (online)
		cpumask_set_cpu(cpu, to_cpumask(cpu_online_bits));
	else
		cpumask_clear_cpu(cpu, to_cpumask(cpu_online_bits));
}

// ARM10C 20130907 cpu = 0, active = 1
// cpu_active_bits[ 0 ] = 1 이됨
void set_cpu_active(unsigned int cpu, bool active)
{
	if (active)
		cpumask_set_cpu(cpu, to_cpumask(cpu_active_bits));
	else
		cpumask_clear_cpu(cpu, to_cpumask(cpu_active_bits));
}

void init_cpu_present(const struct cpumask *src)
{
	cpumask_copy(to_cpumask(cpu_present_bits), src);
}

void init_cpu_possible(const struct cpumask *src)
{
	cpumask_copy(to_cpumask(cpu_possible_bits), src);
}

void init_cpu_online(const struct cpumask *src)
{
	cpumask_copy(to_cpumask(cpu_online_bits), src);
}
