/*
 * linux/kernel/time/tick-common.c
 *
 * This file contains the base functions to manage periodic tick
 * related events.
 *
 * Copyright(C) 2005-2006, Thomas Gleixner <tglx@linutronix.de>
 * Copyright(C) 2005-2007, Red Hat, Inc., Ingo Molnar
 * Copyright(C) 2006-2007, Timesys Corp., Thomas Gleixner
 *
 * This code is licenced under the GPL version 2. For details see
 * kernel-base/COPYING.
 */
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/profile.h>
#include <linux/sched.h>
#include <linux/module.h>

#include <asm/irq_regs.h>

#include "tick-internal.h"

/*
 * Tick devices
 */
// ARM10C 20150411
// ARM10C 20150523
DEFINE_PER_CPU(struct tick_device, tick_cpu_device);
/*
 * Tick next event: keeps track of the tick time
 */
// ARM10C 20150418
ktime_t tick_next_period;
// ARM10C 20150418
ktime_t tick_period;

/*
 * tick_do_timer_cpu is a timer core internal variable which holds the CPU NR
 * which is responsible for calling do_timer(), i.e. the timekeeping stuff. This
 * variable has two functions:
 *
 * 1) Prevent a thundering herd issue of a gazillion of CPUs trying to grab the
 *    timekeeping lock all at once. Only the CPU which is assigned to do the
 *    update is handling it.
 *
 * 2) Hand off the duty in the NOHZ idle case by setting the value to
 *    TICK_DO_TIMER_NONE, i.e. a non existing CPU. So the next cpu which looks
 *    at it will take over and keep the time keeping alive.  The handover
 *    procedure also covers cpu hotplug.
 */
// ARM10C 20150418
// TICK_DO_TIMER_BOOT: -2
// tick_do_timer_cpu: -2
int tick_do_timer_cpu __read_mostly = TICK_DO_TIMER_BOOT;

/*
 * Debugging: see timer_list.c
 */
struct tick_device *tick_get_device(int cpu)
{
	return &per_cpu(tick_cpu_device, cpu);
}

/**
 * tick_is_oneshot_available - check for a oneshot capable event device
 */
int tick_is_oneshot_available(void)
{
	struct clock_event_device *dev = __this_cpu_read(tick_cpu_device.evtdev);

	if (!dev || !(dev->features & CLOCK_EVT_FEAT_ONESHOT))
		return 0;
	if (!(dev->features & CLOCK_EVT_FEAT_C3STOP))
		return 1;
	return tick_broadcast_oneshot_available();
}

/*
 * Periodic tick
 */
static void tick_periodic(int cpu)
{
	if (tick_do_timer_cpu == cpu) {
		write_seqlock(&jiffies_lock);

		/* Keep track of the next tick event */
		tick_next_period = ktime_add(tick_next_period, tick_period);

		do_timer(1);
		write_sequnlock(&jiffies_lock);
	}

	update_process_times(user_mode(get_irq_regs()));
	profile_tick(CPU_PROFILING);
}

/*
 * Event handler for periodic ticks
 */
// ARM10C 20150418
void tick_handle_periodic(struct clock_event_device *dev)
{
	int cpu = smp_processor_id();
	ktime_t next;

	tick_periodic(cpu);

	if (dev->mode != CLOCK_EVT_MODE_ONESHOT)
		return;
	/*
	 * Setup the next period for devices, which do not have
	 * periodic mode:
	 */
	next = ktime_add(dev->next_event, tick_period);
	for (;;) {
		if (!clockevents_program_event(dev, next, false))
			return;
		/*
		 * Have to be careful here. If we're in oneshot mode,
		 * before we call tick_periodic() in a loop, we need
		 * to be sure we're using a real hardware clocksource.
		 * Otherwise we could get trapped in an infinite
		 * loop, as the tick_periodic() increments jiffies,
		 * when then will increment time, posibly causing
		 * the loop to trigger again and again.
		 */
		if (timekeeping_valid_for_hres())
			tick_periodic(cpu);
		next = ktime_add(next, tick_period);
	}
}

/*
 * Setup the device for a periodic tick
 */
// ARM10C 20150418
// newdev: [pcp0] &(&percpu_mct_tick)->evt, 0
void tick_setup_periodic(struct clock_event_device *dev, int broadcast)
{
	// dev: [pcp0] &(&percpu_mct_tick)->evt, broadcast: 0
	tick_set_periodic_handler(dev, broadcast);

	// tick_set_periodic_handler에서 한일:
	// [pcp0] (&(&percpu_mct_tick)->evt)->event_handler: tick_handle_periodic

	/* Broadcast setup ? */
	// dev: [pcp0] &(&percpu_mct_tick)->evt,
	// tick_device_is_functional([pcp0] &(&percpu_mct_tick)->evt): 1
	if (!tick_device_is_functional(dev))
		return;

// 2015/04/18 종료
// 2015/05/09 시작

	// dev->features: [pcp0] (&(&percpu_mct_tick)->evt)->features: 0x3, CLOCK_EVT_FEAT_ONESHOT: 0x000002,
	// tick_broadcast_oneshot_active(): 0
	if ((dev->features & CLOCK_EVT_FEAT_PERIODIC) &&
	    !tick_broadcast_oneshot_active()) {
		// dev: [pcp0] &(&percpu_mct_tick)->evt, CLOCK_EVT_MODE_PERIODIC: 2
		clockevents_set_mode(dev, CLOCK_EVT_MODE_PERIODIC);

		// clockevents_set_mode에서 한일:
		// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
		// 동작하지 않도록 변경함
		// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
		//
		// register L_ICNTB 에 0x8001D4C0 write함
		// local timer 0 의 interrupt count buffer 값을 120000 (0x1D4C0) write 하고
		// interrupt manual update를 enable 시킴
		//
		// register L_INT_ENB 에 0x1 write함
		// local timer 0 의 ICNTEIE 값을 0x1을 write 하여 L0_INTCNT 값이 0 이 되었을 때
		// interrupt counter expired interrupt 가 발생하도록 함
		//
		// register L_TCON 에 0x7 write함
		// local timer 0 의 interrupt type을 interval mode로 설정하고 interrupt, timer 를 start 시킴
		//
		// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2
	} else {
		unsigned long seq;
		ktime_t next;

		do {
			seq = read_seqbegin(&jiffies_lock);
			next = tick_next_period;
		} while (read_seqretry(&jiffies_lock, seq));

		clockevents_set_mode(dev, CLOCK_EVT_MODE_ONESHOT);

		for (;;) {
			if (!clockevents_program_event(dev, next, false))
				return;
			next = ktime_add(next, tick_period);
		}
	}
}

/*
 * Setup the tick device
 */
// ARM10C 20150418
// td: [pcp0] &tick_cpu_device, newdev: [pcp0] &(&percpu_mct_tick)->evt, cpu: 0, cpumask_of(0): &cpu_bit_bitmap[1][0]
static void tick_setup_device(struct tick_device *td,
			      struct clock_event_device *newdev, int cpu,
			      const struct cpumask *cpumask)
{
	ktime_t next_event;
	void (*handler)(struct clock_event_device *) = NULL;
	// handler: NULL

	/*
	 * First device setup ?
	 */
	// td->evtdev: [pcp0] (&tick_cpu_device)->evtdev: NULL
	if (!td->evtdev) {
		/*
		 * If no cpu took the do_timer update, assign it to
		 * this cpu:
		 */
		// tick_do_timer_cpu: -2, TICK_DO_TIMER_BOOT: -2
		if (tick_do_timer_cpu == TICK_DO_TIMER_BOOT) {
			// cpu: 0, tick_nohz_full_cpu(0): false
			if (!tick_nohz_full_cpu(cpu))
				// tick_do_timer_cpu: -2, cpu: 0
				tick_do_timer_cpu = cpu;
				// tick_do_timer_cpu: 0
			else
				tick_do_timer_cpu = TICK_DO_TIMER_NONE;

			// ktime_get(): (ktime_t) { .tv64 = 0}
			tick_next_period = ktime_get();
			// tick_next_period.tv64: 0

			// NSEC_PER_SEC: 1000000000L, HZ: 100
			// ktime_set(0, 10000000): (ktime_t) { .tv64 = 10000000}
			tick_period = ktime_set(0, NSEC_PER_SEC / HZ);
			// tick_period.tv64: 10000000
		}

		/*
		 * Startup in periodic mode first.
		 */
		// td->mode: [pcp0] (&tick_cpu_device)->mode, TICKDEV_MODE_PERIODIC: 0
		td->mode = TICKDEV_MODE_PERIODIC;
		// td->mode: [pcp0] (&tick_cpu_device)->mode: 0
	} else {
		handler = td->evtdev->event_handler;
		next_event = td->evtdev->next_event;
		td->evtdev->event_handler = clockevents_handle_noop;
	}

	// td->evtdev: [pcp0] (&tick_cpu_device)->evtdev: NULL, newdev: [pcp0] &(&percpu_mct_tick)->evt
	td->evtdev = newdev;
	// td->evtdev: [pcp0] (&tick_cpu_device)->evtdev: [pcp0] &(&percpu_mct_tick)->evt

	/*
	 * When the device is not per cpu, pin the interrupt to the
	 * current cpu:
	 */
	// newdev->cpumask: [pcp0] (&(&percpu_mct_tick)->evt)->cpumask: &cpu_bit_bitmap[1][0],
	// cpumask: &cpu_bit_bitmap[1][0]
	// cpumask_equal(&cpu_bit_bitmap[1][0], &cpu_bit_bitmap[1][0]): 1
	if (!cpumask_equal(newdev->cpumask, cpumask))
		irq_set_affinity(newdev->irq, cpumask);

	/*
	 * When global broadcasting is active, check if the current
	 * device is registered as a placeholder for broadcast mode.
	 * This allows us to handle this x86 misfeature in a generic
	 * way. This function also returns !=0 when we keep the
	 * current active broadcast state for this CPU.
	 */
	// newdev: [pcp0] &(&percpu_mct_tick)->evt, cpu: 0
	// tick_device_uses_broadcast([pcp0] &(&percpu_mct_tick)->evt, 0): 0
	if (tick_device_uses_broadcast(newdev, cpu))
		return;

	// td->mode: [pcp0] (&tick_cpu_device)->mode: 0, TICKDEV_MODE_PERIODIC: 0
	if (td->mode == TICKDEV_MODE_PERIODIC)
		// newdev: [pcp0] &(&percpu_mct_tick)->evt
		tick_setup_periodic(newdev, 0);

		// tick_setup_periodic에서 한일:
		// [pcp0] (&(&percpu_mct_tick)->evt)->event_handler: tick_handle_periodic
		// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2
		//
		// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
		// 동작하지 않도록 변경함
		// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
		//
		// register L_ICNTB 에 0x8001D4C0 write함
		// local timer 0 의 interrupt count buffer 값을 120000 (0x1D4C0) write 하고
		// interrupt manual update를 enable 시킴
		//
		// register L_INT_ENB 에 0x1 write함
		// local timer 0 의 ICNTEIE 값을 0x1을 write 하여 L0_INTCNT 값이 0 이 되었을 때
		// interrupt counter expired interrupt 가 발생하도록 함
		//
		// register L_TCON 에 0x7 write함
		// local timer 0 의 interrupt type을 interval mode로 설정하고 interrupt, timer 를 start 시킴
	else
		tick_setup_oneshot(newdev, handler, next_event);
}

void tick_install_replacement(struct clock_event_device *newdev)
{
	struct tick_device *td = &__get_cpu_var(tick_cpu_device);
	int cpu = smp_processor_id();

	clockevents_exchange_device(td->evtdev, newdev);
	tick_setup_device(td, newdev, cpu, cpumask_of(cpu));
	if (newdev->features & CLOCK_EVT_FEAT_ONESHOT)
		tick_oneshot_notify();
}

// ARM10C 20150411
// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: [pcp0] &(&percpu_mct_tick)->evt, cpu: 0
// ARM10C 20150523
// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: &mct_comp_device, cpu: 0
static bool tick_check_percpu(struct clock_event_device *curdev,
			      struct clock_event_device *newdev, int cpu)
{
	// cpu: 0, newdev->cpumask: [pcp0] (&(&percpu_mct_tick)->evt)->cpumask: &cpu_bit_bitmap[1][0]
	// cpumask_test_cpu(0, &cpu_bit_bitmap[1][0]): 1
	// cpu: 0, newdev->cpumask: (&mct_comp_device)->cpumask: &cpu_bit_bitmap[1][0]
	// cpumask_test_cpu(0, &cpu_bit_bitmap[1][0]): 1
	if (!cpumask_test_cpu(cpu, newdev->cpumask))
		return false;

	// newdev->cpumask: [pcp0] (&(&percpu_mct_tick)->evt)->cpumask: &cpu_bit_bitmap[1][0],
	// cpu: 0, cpumask_of(0): &cpu_bit_bitmap[1][0]
	// cpumask_equal(&cpu_bit_bitmap[1][0], &cpu_bit_bitmap[1][0]): 1
	// newdev->cpumask: (&mct_comp_device)->cpumask: &cpu_bit_bitmap[1][0],
	// cpu: 0, cpumask_of(0): &cpu_bit_bitmap[1][0]
	// cpumask_equal(&cpu_bit_bitmap[1][0], &cpu_bit_bitmap[1][0]): 1
	if (cpumask_equal(newdev->cpumask, cpumask_of(cpu)))
		return true;
		// return true
		// return true

	/* Check if irq affinity can be set */
	if (newdev->irq >= 0 && !irq_can_set_affinity(newdev->irq))
		return false;
	/* Prefer an existing cpu local device */
	if (curdev && cpumask_equal(curdev->cpumask, cpumask_of(cpu)))
		return false;
	return true;
}

// ARM10C 20150411
// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: [pcp0] &(&percpu_mct_tick)->evt
// ARM10C 20150523
// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: &mct_comp_device
static bool tick_check_preferred(struct clock_event_device *curdev,
				 struct clock_event_device *newdev)
{
	/* Prefer oneshot capable device */
	// newdev->features: [pcp0] (&(&percpu_mct_tick)->evt)->features: 0x3, CLOCK_EVT_FEAT_ONESHOT: 0x000002
	// newdev->features: (&mct_comp_device)->features: 0x3, CLOCK_EVT_FEAT_ONESHOT: 0x000002
	if (!(newdev->features & CLOCK_EVT_FEAT_ONESHOT)) {
		if (curdev && (curdev->features & CLOCK_EVT_FEAT_ONESHOT))
			return false;
		if (tick_oneshot_mode_active())
			return false;
	}

	/*
	 * Use the higher rated one, but prefer a CPU local device with a lower
	 * rating than a non-CPU local device
	 */
	// curdev: [pcp0] (&tick_cpu_device)->evtdev: NULL, newdev->rating: [pcp0] (&(&percpu_mct_tick)->evt)->rating,
	// curdev->rating: [pcp0] (&tick_cpu_device)->evtdev->rating, curdev->cpumask: [pcp0] (&tick_cpu_device)->evtdev->cpumask,
	// newdev->cpumask: [pcp0] (&(&percpu_mct_tick)->evt)->cpumask,
	// curdev: [pcp0] (&tick_cpu_device)->evtdev: [pcp0] &(&percpu_mct_tick)->evt,
	// newdev->rating: (&mct_comp_device)->rating: 250,
	// curdev->rating: [pcp0] (&tick_cpu_device)->evtdev->rating: 450,
	// curdev->cpumask: [pcp0] (&tick_cpu_device)->evtdev->cpumask: &cpu_bit_bitmap[1][0],
	// newdev->cpumask: (&mct_comp_device)->cpumask: &cpu_bit_bitmap[1][0],
	// cpumask_equal(&cpu_bit_bitmap[1][0], &cpu_bit_bitmap[1][0]): 1
	return !curdev ||
		newdev->rating > curdev->rating ||
	       !cpumask_equal(curdev->cpumask, newdev->cpumask);
	// retun 1
	// retun 0
}

/*
 * Check whether the new device is a better fit than curdev. curdev
 * can be NULL !
 */
bool tick_check_replacement(struct clock_event_device *curdev,
			    struct clock_event_device *newdev)
{
	if (tick_check_percpu(curdev, newdev, smp_processor_id()))
		return false;

	return tick_check_preferred(curdev, newdev);
}

/*
 * Check, if the new registered device should be used. Called with
 * clockevents_lock held and interrupts disabled.
 */
// ARM10C 20150411
// dev: [pcp0] &(&percpu_mct_tick)->evt
// ARM10C 20150523
// dev: &mct_comp_device
void tick_check_new_device(struct clock_event_device *newdev)
{
	struct clock_event_device *curdev;
	struct tick_device *td;
	int cpu;

	// smp_processor_id(): 0
	// smp_processor_id(): 0
	cpu = smp_processor_id();
	// cpu: 0
	// cpu: 0

	// cpu: 0, newdev->cpumask: [pcp0] (&(&percpu_mct_tick)->evt)->cpumask: &cpu_bit_bitmap[1][0]
	// cpumask_test_cpu(0, &cpu_bit_bitmap[1][0]): 1
	// cpu: 0, newdev->cpumask: (&mct_comp_device)->cpumask: &cpu_bit_bitmap[1][0]
	// cpumask_test_cpu(0, &cpu_bit_bitmap[1][0]): 1
	if (!cpumask_test_cpu(cpu, newdev->cpumask))
		goto out_bc;

	// cpu: 0, per_cpu(tick_cpu_device, 0): [pcp0] tick_cpu_device
	// cpu: 0, per_cpu(tick_cpu_device, 0): [pcp0] tick_cpu_device
	td = &per_cpu(tick_cpu_device, cpu);
	// td: [pcp0] &tick_cpu_device
	// td: [pcp0] &tick_cpu_device

	// td->evtdev: [pcp0] (&tick_cpu_device)->evtdev
	// td->evtdev: [pcp0] (&tick_cpu_device)->evtdev
	curdev = td->evtdev;
	// curdev: [pcp0] (&tick_cpu_device)->evtdev
	// curdev: [pcp0] (&tick_cpu_device)->evtdev

	/* cpu local device ? */
	// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: [pcp0] &(&percpu_mct_tick)->evt, cpu: 0
	// tick_check_percpu([pcp0] (&tick_cpu_device)->evtdev, [pcp0] &(&percpu_mct_tick)->evt, 0): true
	// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: &mct_comp_device, cpu: 0
	// tick_check_percpu([pcp0] (&tick_cpu_device)->evtdev, &mct_comp_device, 0): true
	if (!tick_check_percpu(curdev, newdev, cpu))
		goto out_bc;

	/* Preference decision */
	// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: [pcp0] &(&percpu_mct_tick)->evt
	// tick_check_preferred([pcp0] (&tick_cpu_device)->evtdev, [pcp0] &(&percpu_mct_tick)->evt): 1
	// curdev: [pcp0] (&tick_cpu_device)->evtdev, newdev: &mct_comp_device
	// tick_check_preferred([pcp0] (&tick_cpu_device)->evtdev, &mct_comp_device): 0
	if (!tick_check_preferred(curdev, newdev))
		goto out_bc;
		// goto out_bc 수행

	// newdev->owner: [pcp0] (&(&percpu_mct_tick)->evt)->owner,
	// try_module_get([pcp0] (&(&percpu_mct_tick)->evt)->owner): true
	if (!try_module_get(newdev->owner))
		return;

	/*
	 * Replace the eventually existing device by the new
	 * device. If the current device is the broadcast device, do
	 * not give it back to the clockevents layer !
	 */
	// curdev: [pcp0] (&tick_cpu_device)->evtdev: NULL
	// tick_is_broadcast_device([pcp0] (&tick_cpu_device)->evtdev): 0
	if (tick_is_broadcast_device(curdev)) {
		clockevents_shutdown(curdev);
		curdev = NULL;
	}

	// curdev: [pcp0] (&tick_cpu_device)->evtdev: NULL, newdev: [pcp0] &(&percpu_mct_tick)->evt
	// clockevents_exchange_device(NULL, [pcp0] &(&percpu_mct_tick)->evt)
	clockevents_exchange_device(curdev, newdev);

	// clockevents_exchange_device에서 한일:
	// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
	// 동작하지 않도록 변경함
	// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
	//
	// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 1
	// [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x7FFFFFFFFFFFFFFF

	// td: [pcp0] &tick_cpu_device, newdev: [pcp0] &(&percpu_mct_tick)->evt, cpu: 0, cpumask_of(0): &cpu_bit_bitmap[1][0]
	tick_setup_device(td, newdev, cpu, cpumask_of(cpu));

	// tick_setup_device에서 한일:
	//
	// tick_do_timer_cpu: 0
	// tick_next_period.tv64: 0
	// tick_period.tv64: 10000000
	//
	// [pcp0] (&tick_cpu_device)->mode: 0
	// [pcp0] (&tick_cpu_device)->evtdev: [pcp0] &(&percpu_mct_tick)->evt
	//
	// [pcp0] (&(&percpu_mct_tick)->evt)->event_handler: tick_handle_periodic
	// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2
	//
	// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
	// 동작하지 않도록 변경함
	// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
	//
	// register L_ICNTB 에 0x8001D4C0 write함
	// local timer 0 의 interrupt count buffer 값을 120000 (0x1D4C0) write 하고
	// interrupt manual update를 enable 시킴
	//
	// register L_INT_ENB 에 0x1 write함
	// local timer 0 의 ICNTEIE 값을 0x1을 write 하여 L0_INTCNT 값이 0 이 되었을 때
	// interrupt counter expired interrupt 가 발생하도록 함
	//
	// register L_TCON 에 0x7 write함
	// local timer 0 의 interrupt type을 interval mode로 설정하고 interrupt, timer 를 start 시킴

	// newdev->features: [pcp0] (&(&percpu_mct_tick)->evt)->features: 0x3, CLOCK_EVT_FEAT_ONESHOT: 0x000002
	if (newdev->features & CLOCK_EVT_FEAT_ONESHOT)
		tick_oneshot_notify();

		// tick_oneshot_notify에서 한일:
		// [pcp0] (&tick_cpu_sched)->check_clocks: 1

	return;
	// return 수행

out_bc:
	/*
	 * Can the new device be used as a broadcast device ?
	 */
	// newdev: &mct_comp_device
	tick_install_broadcast_device(newdev);

	// tick_install_broadcast_device에서 한일:
	// register G_TCON 에 0x100 write함
	// global timer enable 의 값을 1로 write 함
	//
	// register G_INT_ENB 에 0x0 write함
	// global timer interrupt enable 의 값을 0로 write 함
	//
	// comparator 0의 auto increment0, comp0 enable,comp0 interrupt enable 값을
	// 0으로 clear 하여 comparator 0를 동작하지 않도록 함
	//
	// (&mct_comp_device)->mode: 1
	// (&mct_comp_device)->next_event.tv64: 0x7FFFFFFFFFFFFFFF
	//
	// tick_broadcast_device.evtdev: &mct_comp_device
	// [pcp0] &(&tick_cpu_sched)->check_clocks: 0xf
}

/*
 * Transfer the do_timer job away from a dying cpu.
 *
 * Called with interrupts disabled.
 */
void tick_handover_do_timer(int *cpup)
{
	if (*cpup == tick_do_timer_cpu) {
		int cpu = cpumask_first(cpu_online_mask);

		tick_do_timer_cpu = (cpu < nr_cpu_ids) ? cpu :
			TICK_DO_TIMER_NONE;
	}
}

/*
 * Shutdown an event device on a given cpu:
 *
 * This is called on a life CPU, when a CPU is dead. So we cannot
 * access the hardware device itself.
 * We just set the mode and remove it from the lists.
 */
void tick_shutdown(unsigned int *cpup)
{
	struct tick_device *td = &per_cpu(tick_cpu_device, *cpup);
	struct clock_event_device *dev = td->evtdev;

	td->mode = TICKDEV_MODE_PERIODIC;
	if (dev) {
		/*
		 * Prevent that the clock events layer tries to call
		 * the set mode function!
		 */
		dev->mode = CLOCK_EVT_MODE_UNUSED;
		clockevents_exchange_device(dev, NULL);
		dev->event_handler = clockevents_handle_noop;
		td->evtdev = NULL;
	}
}

void tick_suspend(void)
{
	struct tick_device *td = &__get_cpu_var(tick_cpu_device);

	clockevents_shutdown(td->evtdev);
}

void tick_resume(void)
{
	struct tick_device *td = &__get_cpu_var(tick_cpu_device);
	int broadcast = tick_resume_broadcast();

	clockevents_set_mode(td->evtdev, CLOCK_EVT_MODE_RESUME);

	if (!broadcast) {
		if (td->mode == TICKDEV_MODE_PERIODIC)
			tick_setup_periodic(td->evtdev, 0);
		else
			tick_resume_oneshot();
	}
}

/**
 * tick_init - initialize the tick control
 */
// ARM10C 20150103
void __init tick_init(void)
{
	tick_broadcast_init();
	
	// tick_broadcast_init에서 한일:
	// tick_broadcast_mask.bits[0]: 0
	// tick_broadcast_on.bits[0]: 0
	// tmpmask.bits[0]: 0
	// tick_broadcast_oneshot_mask.bits[0]: 0
	// tick_broadcast_pending_mask.bits[0]: 0
	// tick_broadcast_force_mask.bits[0]: 0
}
