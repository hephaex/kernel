/*
 * linux/kernel/time/clockevents.c
 *
 * This file contains functions which manage clock event devices.
 *
 * Copyright(C) 2005-2006, Thomas Gleixner <tglx@linutronix.de>
 * Copyright(C) 2005-2007, Red Hat, Inc., Ingo Molnar
 * Copyright(C) 2006-2007, Timesys Corp., Thomas Gleixner
 *
 * This code is licenced under the GPL version 2. For details see
 * kernel-base/COPYING.
 */

#include <linux/clockchips.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/device.h>

#include "tick-internal.h"

/* The registered clock event devices */
// ARM10C 20150411
static LIST_HEAD(clockevent_devices);
// ARM10C 20150509
// ARM10C 20150523
static LIST_HEAD(clockevents_released);

/* Protection for the above */
// ARM10C 20150411
// DEFINE_RAW_SPINLOCK(clockevents_lock):
// raw_spinlock_t clockevents_lock =
// (raw_spinlock_t)
// {
//    .raw_lock = { { 0 } },
//    .magic = 0xdead4ead,
//    .owner_cpu = -1,
//    .owner = 0xffffffff,
// }
static DEFINE_RAW_SPINLOCK(clockevents_lock);
/* Protection for unbind operations */
static DEFINE_MUTEX(clockevents_mutex);

struct ce_unbind {
	struct clock_event_device *ce;
	int res;
};

// ARM10C 20150411
// dev->min_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ticks: 0xf, dev: [pcp0] &(&percpu_mct_tick)->evt, false
// ARM10C 20150411
// dev->max_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ticks: 0x7fffffff, dev: [pcp0] &(&percpu_mct_tick)->evt, true
// ARM10C 20150523
// dev->min_delta_ticks: (&mct_comp_device)->min_delta_ticks: 0xf, dev: &mct_comp_device, false
// ARM10C 20150523
// dev->max_delta_ticks: (&mct_comp_device)->max_delta_ticks: 0xffffffff, dev: &mct_comp_device, true
static u64 cev_delta2ns(unsigned long latch, struct clock_event_device *evt,
			bool ismax)
{
	// latch: 0xf, evt->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32
	// latch: 0x7fffffff, evt->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32
	// latch: 0xf, evt->shift: (&mct_comp_device)->shift: 31
	// latch: 0xffffffff, evt->shift: (&mct_comp_device)->shift: 31
	u64 clc = (u64) latch << evt->shift;
	// clc: 0xf00000000
	// clc: 0x7fffffff00000000
	// clc: 0x780000000
	// clc: 0x7FFFFFFF80000000

	u64 rnd;

	// evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// evt->mult: (&mct_comp_device)->mult: 0x3126E98
	// evt->mult: (&mct_comp_device)->mult: 0x3126E98
	if (unlikely(!evt->mult)) {
		evt->mult = 1;
		WARN_ON(1);
	}

	// evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// evt->mult: (&mct_comp_device)->mult: 0x3126E98
	// evt->mult: (&mct_comp_device)->mult: 0x3126E98
	rnd = (u64) evt->mult - 1;
	// rnd: 0x3126E97
	// rnd: 0x3126E97
	// rnd: 0x3126E97
	// rnd: 0x3126E97

	/*
	 * Upper bound sanity check. If the backwards conversion is
	 * not equal latch, we know that the above shift overflowed.
	 */
	// clc: 0xf00000000, evt->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32, latch: 0xf
	// clc: 0x7fffffff00000000, evt->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32, latch: 0x7fffffff
	// clc: 0x780000000, evt->shift: (&mct_comp_device)->shift: 31, latch: 0xf
	// clc: 0x7FFFFFFF80000000, evt->shift: (&mct_comp_device)->shift: 31, latch: 0xffffffff
	if ((clc >> evt->shift) != (u64)latch)
		clc = ~0ULL;

	/*
	 * Scaled math oddities:
	 *
	 * For mult <= (1 << shift) we can safely add mult - 1 to
	 * prevent integer rounding loss. So the backwards conversion
	 * from nsec to device ticks will be correct.
	 *
	 * For mult > (1 << shift), i.e. device frequency is > 1GHz we
	 * need to be careful. Adding mult - 1 will result in a value
	 * which when converted back to device ticks can be larger
	 * than latch by up to (mult - 1) >> shift. For the min_delta
	 * calculation we still want to apply this in order to stay
	 * above the minimum device ticks limit. For the upper limit
	 * we would end up with a latch value larger than the upper
	 * limit of the device, so we omit the add to stay below the
	 * device upper boundary.
	 *
	 * Also omit the add if it would overflow the u64 boundary.
	 */
	// clc: 0xf00000000, rnd: 0x3126E97, ismax: false,
	// evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98,
	// evt->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32
	// clc: 0x7fffffff00000000, rnd: 0x3126E97, ismax: true,
	// evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98,
	// evt->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32
	// clc: 0x780000000, rnd: 0x3126E97, ismax: false,
	// evt->mult: (&mct_comp_device)->mult: 0x3126E98, evt->shift: (&mct_comp_device)->shift: 31
	// clc: 0x7FFFFFFF80000000, rnd: 0x3126E97, ismax: true,
	// evt->mult: (&mct_comp_device)->mult: 0x3126E98, evt->shift: (&mct_comp_device)->shift: 31
	if ((~0ULL - clc > rnd) &&
	    (!ismax || evt->mult <= (1U << evt->shift)))
		// clc: 0xf00000000, rnd: 0x3126E97
		// clc: 0x7fffffff00000000, rnd: 0x3126E97
		// clc: 0x780000000, rnd: 0x3126E97
		// clc: 0x7FFFFFFF80000000, rnd: 0x3126E97
		clc += rnd;
		// clc: 0xf03126E97
		// clc: 0x7fffffff03126E97
		// clc: 0x783126E97
		// clc: 0x7FFFFFFF83126E97

	// clc: 0xf03126E97, evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// clc: 0x7fffffff03126E97, evt->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// clc: 0x783126E97, evt->mult: (&mct_comp_device)->mult: 0x3126E98
	// clc: 0x7FFFFFFF83126E97, evt->mult: (&mct_comp_device)->mult: 0x3126E98
	do_div(clc, evt->mult);
	// clc: 0x4E2
	// clc: 0x29AAAAA444
	// clc: 0x271
	// clc: 0x29AAAAA46E

	/* Deltas less than 1usec are pointless noise */
	// clc: 0x4E2
	// clc: 0x29AAAAA444
	// clc: 0x271
	// clc: 0x29AAAAA46E
	return clc > 1000 ? clc : 1000;
	// return 0x4E2
	// return 0x29AAAAA444
	// return 0x3E8
	// return 0x29AAAAA46E
}

/**
 * clockevents_delta2ns - Convert a latch value (device ticks) to nanoseconds
 * @latch:	value to convert
 * @evt:	pointer to clock event device descriptor
 *
 * Math helper, returns latch value converted to nanoseconds (bound checked)
 */
u64 clockevent_delta2ns(unsigned long latch, struct clock_event_device *evt)
{
	return cev_delta2ns(latch, evt, false);
}
EXPORT_SYMBOL_GPL(clockevent_delta2ns);

/**
 * clockevents_set_mode - set the operating mode of a clock event device
 * @dev:	device to modify
 * @mode:	new mode
 *
 * Must be called with interrupts disabled !
 */
// ARM10C 20150411
// dev: [pcp0] &(&percpu_mct_tick)->evt, CLOCK_EVT_MODE_SHUTDOWN: 1
// ARM10C 20150509
// dev: [pcp0] &(&percpu_mct_tick)->evt, CLOCK_EVT_MODE_PERIODIC: 2
// ARM10C 20150523
// dev: &mct_comp_device, CLOCK_EVT_MODE_SHUTDOWN: 1
void clockevents_set_mode(struct clock_event_device *dev,
				 enum clock_event_mode mode)
{
// 2015/04/11 종료
// 2015/04/18 시작

	// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 0, mode: 1
	// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 1, mode: 2
	// dev->mode: (&mct_comp_device)->mode: 0, mode: 1
	if (dev->mode != mode) {
		// dev->set_mode: [pcp0] (&(&percpu_mct_tick)->evt)->set_mode: exynos4_tick_set_mode
		// mode: 1, dev: [pcp0] &(&percpu_mct_tick)->evt
		// exynos4_tick_set_mode(1, [pcp0] &(&percpu_mct_tick)->evt)
		// dev->set_mode: [pcp0] (&(&percpu_mct_tick)->evt)->set_mode: exynos4_tick_set_mode
		// mode: 2, dev: [pcp0] &(&percpu_mct_tick)->evt
		// exynos4_tick_set_mode(2, [pcp0] &(&percpu_mct_tick)->evt)
		// dev->set_mode: (&mct_comp_device)->set_mode: exynos4_comp_set_mode
		// mode: 1, dev: &mct_comp_device
		// exynos4_comp_set_mode(1, &mct_comp_device)
		dev->set_mode(mode, dev);

		// exynos4_tick_set_mode(1)에서 한일:
		// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
		// 동작하지 않도록 변경함
		// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임

		// exynos4_tick_set_mode(2)에서 한일:
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

		// exynos4_comp_set_mode(1)에서 한일:
		// register G_TCON 에 0x100 write함
		// global timer enable 의 값을 1로 write 함
		//
		// register G_INT_ENB 에 0x0 write함
		// global timer interrupt enable 의 값을 0로 write 함
		//
		// comparator 0의 auto increment0, comp0 enable,comp0 interrupt enable 값을
		// 0으로 clear 하여 comparator 0를 동작하지 않도록 함

		// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 0, mode: 1
		// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 1, mode: 2
		// dev->mode: (&mct_comp_device)->mode: 0, mode: 1
		dev->mode = mode;
		// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 1
		// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2
		// dev->mode: (&mct_comp_device)->mode: 1

		/*
		 * A nsec2cyc multiplicator of 0 is invalid and we'd crash
		 * on it, so fix it up and emit a warning:
		 */
		// mode: 1, CLOCK_EVT_MODE_ONESHOT: 3
		// mode: 2, CLOCK_EVT_MODE_ONESHOT: 3
		// mode: 1, CLOCK_EVT_MODE_ONESHOT: 3
		if (mode == CLOCK_EVT_MODE_ONESHOT) {
			if (unlikely(!dev->mult)) {
				dev->mult = 1;
				WARN_ON(1);
			}
		}
	}
}

/**
 * clockevents_shutdown - shutdown the device and clear next_event
 * @dev:	device to shutdown
 */
// ARM10C 20150411
// new: [pcp0] &(&percpu_mct_tick)->evt
// ARM10C 20150523
// new: &mct_comp_device
void clockevents_shutdown(struct clock_event_device *dev)
{
	// dev: [pcp0] &(&percpu_mct_tick)->evt, CLOCK_EVT_MODE_SHUTDOWN: 1
	// clockevents_set_mode([pcp0] &(&percpu_mct_tick)->evt, 1)
	// dev: &mct_comp_device, CLOCK_EVT_MODE_SHUTDOWN: 1
	// clockevents_set_mode(&mct_comp_device, 1)
	clockevents_set_mode(dev, CLOCK_EVT_MODE_SHUTDOWN);

	// clockevents_set_mode에서 한일:
	// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
	// 동작하지 않도록 변경함
	// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
	//
	// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 1

	// clockevents_set_mode에서 한일:
	// register G_TCON 에 0x100 write함
	// global timer enable 의 값을 1로 write 함
	//
	// register G_INT_ENB 에 0x0 write함
	// global timer interrupt enable 의 값을 0로 write 함
	//
	// comparator 0의 auto increment0, comp0 enable,comp0 interrupt enable 값을
	// 0으로 clear 하여 comparator 0를 동작하지 않도록 함
	//
	// dev->mode: (&mct_comp_device)->mode: 1

	// dev->next_event.tv64: [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64, KTIME_MAX: 0x7FFFFFFFFFFFFFFF
	// dev->next_event.tv64: (&mct_comp_device)->next_event.tv64, KTIME_MAX: 0x7FFFFFFFFFFFFFFF
	dev->next_event.tv64 = KTIME_MAX;
	// dev->next_event.tv64: [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x7FFFFFFFFFFFFFFF
	// dev->next_event.tv64: (&mct_comp_device)->next_event.tv64: 0x7FFFFFFFFFFFFFFF
}

#ifdef CONFIG_GENERIC_CLOCKEVENTS_MIN_ADJUST

/* Limit min_delta to a jiffie */
#define MIN_DELTA_LIMIT		(NSEC_PER_SEC / HZ)

/**
 * clockevents_increase_min_delta - raise minimum delta of a clock event device
 * @dev:       device to increase the minimum delta
 *
 * Returns 0 on success, -ETIME when the minimum delta reached the limit.
 */
static int clockevents_increase_min_delta(struct clock_event_device *dev)
{
	/* Nothing to do if we already reached the limit */
	if (dev->min_delta_ns >= MIN_DELTA_LIMIT) {
		printk(KERN_WARNING "CE: Reprogramming failure. Giving up\n");
		dev->next_event.tv64 = KTIME_MAX;
		return -ETIME;
	}

	if (dev->min_delta_ns < 5000)
		dev->min_delta_ns = 5000;
	else
		dev->min_delta_ns += dev->min_delta_ns >> 1;

	if (dev->min_delta_ns > MIN_DELTA_LIMIT)
		dev->min_delta_ns = MIN_DELTA_LIMIT;

	printk(KERN_WARNING "CE: %s increased min_delta_ns to %llu nsec\n",
	       dev->name ? dev->name : "?",
	       (unsigned long long) dev->min_delta_ns);
	return 0;
}

/**
 * clockevents_program_min_delta - Set clock event device to the minimum delay.
 * @dev:	device to program
 *
 * Returns 0 on success, -ETIME when the retry loop failed.
 */
static int clockevents_program_min_delta(struct clock_event_device *dev)
{
	unsigned long long clc;
	int64_t delta;
	int i;

	for (i = 0;;) {
		delta = dev->min_delta_ns;
		dev->next_event = ktime_add_ns(ktime_get(), delta);

		if (dev->mode == CLOCK_EVT_MODE_SHUTDOWN)
			return 0;

		dev->retries++;
		clc = ((unsigned long long) delta * dev->mult) >> dev->shift;
		if (dev->set_next_event((unsigned long) clc, dev) == 0)
			return 0;

		if (++i > 2) {
			/*
			 * We tried 3 times to program the device with the
			 * given min_delta_ns. Try to increase the minimum
			 * delta, if that fails as well get out of here.
			 */
			if (clockevents_increase_min_delta(dev))
				return -ETIME;
			i = 0;
		}
	}
}

#else  /* CONFIG_GENERIC_CLOCKEVENTS_MIN_ADJUST */

/**
 * clockevents_program_min_delta - Set clock event device to the minimum delay.
 * @dev:	device to program
 *
 * Returns 0 on success, -ETIME when the retry loop failed.
 */
static int clockevents_program_min_delta(struct clock_event_device *dev)
{
	unsigned long long clc;
	int64_t delta;

	delta = dev->min_delta_ns;
	dev->next_event = ktime_add_ns(ktime_get(), delta);

	if (dev->mode == CLOCK_EVT_MODE_SHUTDOWN)
		return 0;

	dev->retries++;
	clc = ((unsigned long long) delta * dev->mult) >> dev->shift;
	return dev->set_next_event((unsigned long) clc, dev);
}

#endif /* CONFIG_GENERIC_CLOCKEVENTS_MIN_ADJUST */

/**
 * clockevents_program_event - Reprogram the clock event device.
 * @dev:	device to program
 * @expires:	absolute expiry time (monotonic clock)
 * @force:	program minimum delay if expires can not be set
 *
 * Returns 0 on success, -ETIME when the event is in the past.
 */
// ARM10C 20150620
// dev: [pcp0] tick_cpu_device.evtdev: [pcp0] &(&percpu_mct_tick)->evt, expires.tv64: 0x42C1D83B9ACA00, force: 0
int clockevents_program_event(struct clock_event_device *dev, ktime_t expires,
			      bool force)
{
	unsigned long long clc;
	int64_t delta;
	int rc;

	// expires.tv64: 0x42C1D83B9ACA00
	if (unlikely(expires.tv64 < 0)) {
		WARN_ON_ONCE(1);
		return -ETIME;
	}

	// dev->next_event: [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x7FFFFFFFFFFFFFFF
	// expires.tv64: 0x42C1D83B9ACA00
	dev->next_event = expires;
	// dev->next_event: [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x42C1D83B9ACA00

	// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2, CLOCK_EVT_MODE_SHUTDOWN: 1
	if (dev->mode == CLOCK_EVT_MODE_SHUTDOWN)
		return 0;

	/* Shortcut for clockevent devices that can deal with ktime. */
	// dev->features: [pcp0] (&(&percpu_mct_tick)->evt)->features: 0x3, CLOCK_EVT_FEAT_KTIME: 0x000004
	if (dev->features & CLOCK_EVT_FEAT_KTIME)
		return dev->set_next_ktime(expires, dev);

	// expires.tv64: 0x42C1D83B9ACA00, ktime_get(): (ktime_t) { .tv64 = 0}
	// ktime_sub(0x42C1D83B9ACA00, 0): 0x42C1D83B9ACA00, ktime_to_ns(0x42C1D83B9ACA00): 0x42C1D83B9ACA00
	delta = ktime_to_ns(ktime_sub(expires, ktime_get()));
	// delta: 0x42C1D83B9ACA00

	// delta: 0x42C1D83B9ACA00
	if (delta <= 0)
		return force ? clockevents_program_min_delta(dev) : -ETIME;

	// delta: 0x42C1D83B9ACA00, dev->max_delta_ns: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ns: 0x29AAAAA444
	// min(0x42C1D83B9ACA00, 0x29AAAAA444): 0x29AAAAA444
	delta = min(delta, (int64_t) dev->max_delta_ns);
	// delta: 0x29AAAAA444

	// delta: 0x29AAAAA444, dev->min_delta_ns: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ns: 0x4E2
	// max(0x29AAAAA444, 0x4E2): 0x29AAAAA444
	delta = max(delta, (int64_t) dev->min_delta_ns);
	// delta: 0x29AAAAA444

	// delta: 0x29AAAAA444, dev->mult: [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// dev->shift: [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32
	clc = ((unsigned long long) delta * dev->mult) >> dev->shift;
	// clc: 0x1FFF

	// clc: 0x1FFF, dev: [pcp0] &(&percpu_mct_tick)->evt
	// dev->set_next_event: [pcp0] (&(&percpu_mct_tick)->evt)->set_next_event: exynos4_tick_set_next_event
	// exynos4_tick_set_next_event(0x1FFF, [pcp0] &(&percpu_mct_tick)->evt): 0
	rc = dev->set_next_event((unsigned long) clc, dev);
	// rc: 0

	// exynos4_tick_set_next_event에서 한일:
	// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
	// 동작하지 않도록 변경함
	// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
	//
	// register L_ICNTB 에 0x80001FFF write함
	// local timer 0 의 interrupt count buffer 값을 120000 (0x1FFF) write 하고
	// interrupt manual update를 enable 시킴
	//
	// register L_INT_ENB 에 0x1 write함
	// local timer 0 의 ICNTEIE 값을 0x1을 write 하여 L0_INTCNT 값이 0 이 되었을 때
	// interrupt counter expired interrupt 가 발생하도록 함
	//
	// register L_TCON 에 0x7 write함
	// local timer 0 의 interrupt type을 interval mode로 설정하고 interrupt, timer 를 start 시킴

	// rc: 0, force: 0
	return (rc && force) ? clockevents_program_min_delta(dev) : rc;
	// return 0
}

/*
 * Called after a notify add to make devices available which were
 * released from the notifier call.
 */
// ARM10C 20150509
// ARM10C 20150523
static void clockevents_notify_released(void)
{
	struct clock_event_device *dev;

	// list_empty(&clockevents_released): 1
	while (!list_empty(&clockevents_released)) {
		dev = list_entry(clockevents_released.next,
				 struct clock_event_device, list);
		list_del(&dev->list);
		list_add(&dev->list, &clockevent_devices);
		tick_check_new_device(dev);
	}
}

/*
 * Try to install a replacement clock event device
 */
static int clockevents_replace(struct clock_event_device *ced)
{
	struct clock_event_device *dev, *newdev = NULL;

	list_for_each_entry(dev, &clockevent_devices, list) {
		if (dev == ced || dev->mode != CLOCK_EVT_MODE_UNUSED)
			continue;

		if (!tick_check_replacement(newdev, dev))
			continue;

		if (!try_module_get(dev->owner))
			continue;

		if (newdev)
			module_put(newdev->owner);
		newdev = dev;
	}
	if (newdev) {
		tick_install_replacement(newdev);
		list_del_init(&ced->list);
	}
	return newdev ? 0 : -EBUSY;
}

/*
 * Called with clockevents_mutex and clockevents_lock held
 */
static int __clockevents_try_unbind(struct clock_event_device *ced, int cpu)
{
	/* Fast track. Device is unused */
	if (ced->mode == CLOCK_EVT_MODE_UNUSED) {
		list_del_init(&ced->list);
		return 0;
	}

	return ced == per_cpu(tick_cpu_device, cpu).evtdev ? -EAGAIN : -EBUSY;
}

/*
 * SMP function call to unbind a device
 */
static void __clockevents_unbind(void *arg)
{
	struct ce_unbind *cu = arg;
	int res;

	raw_spin_lock(&clockevents_lock);
	res = __clockevents_try_unbind(cu->ce, smp_processor_id());
	if (res == -EAGAIN)
		res = clockevents_replace(cu->ce);
	cu->res = res;
	raw_spin_unlock(&clockevents_lock);
}

/*
 * Issues smp function call to unbind a per cpu device. Called with
 * clockevents_mutex held.
 */
static int clockevents_unbind(struct clock_event_device *ced, int cpu)
{
	struct ce_unbind cu = { .ce = ced, .res = -ENODEV };

	smp_call_function_single(cpu, __clockevents_unbind, &cu, 1);
	return cu.res;
}

/*
 * Unbind a clockevents device.
 */
int clockevents_unbind_device(struct clock_event_device *ced, int cpu)
{
	int ret;

	mutex_lock(&clockevents_mutex);
	ret = clockevents_unbind(ced, cpu);
	mutex_unlock(&clockevents_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(clockevents_unbind);

/**
 * clockevents_register_device - register a clock event device
 * @dev:	device to register
 */
// ARM10C 20150411
// dev: [pcp0] &(&percpu_mct_tick)->evt
// ARM10C 20150523
// dev: &mct_comp_device
void clockevents_register_device(struct clock_event_device *dev)
{
	unsigned long flags;

	// dev->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 0, CLOCK_EVT_MODE_UNUSED: 0
	// dev->mode: (&mct_comp_device)->mode: 0, CLOCK_EVT_MODE_UNUSED: 0
	BUG_ON(dev->mode != CLOCK_EVT_MODE_UNUSED);

	// evt->cpumask: [pcp0] (&(&percpu_mct_tick)->evt)->cpumask: &cpu_bit_bitmap[1][0]
	// evt->cpumask: (&mct_comp_device)->cpumask: &cpu_bit_bitmap[1][0]
	if (!dev->cpumask) {
		WARN_ON(num_possible_cpus() > 1);
		dev->cpumask = cpumask_of(smp_processor_id());
	}

	raw_spin_lock_irqsave(&clockevents_lock, flags);

	// raw_spin_lock_irqsave에서 한일:
	// clockevents_lock를 사용하여 spin lock을 수행하고 cpsr을 flags에 저장

	// raw_spin_lock_irqsave에서 한일:
	// clockevents_lock를 사용하여 spin lock을 수행하고 cpsr을 flags에 저장

	// &dev->list: [pcp0] (&(&percpu_mct_tick)->evt)->list
	// &dev->list: (&mct_comp_device)->list
	list_add(&dev->list, &clockevent_devices);

	// list_add에서 한일:
	// list clockevent_devices에 [pcp0] (&(&percpu_mct_tick)->evt)->list를 추가함

	// list_add에서 한일:
	// list clockevent_devices에 (&mct_comp_device)->list를 추가함

	// dev: [pcp0] &(&percpu_mct_tick)->evt
	// dev: &mct_comp_device
	tick_check_new_device(dev);

	// tick_check_new_device([pcp0] &(&percpu_mct_tick)->evt)에서 한일:
	// tick_do_timer_cpu: 0
	// tick_next_period.tv64: 0
	// tick_period.tv64: 10000000
	//
	// [pcp0] (&tick_cpu_device)->mode: 0
	// [pcp0] (&tick_cpu_device)->evtdev: [pcp0] &(&percpu_mct_tick)->evt
	//
	// [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x7FFFFFFFFFFFFFFF
	// [pcp0] (&(&percpu_mct_tick)->evt)->event_handler: tick_handle_periodic
	// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2
	//
	// [pcp0] (&tick_cpu_sched)->check_clocks: 1
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

	// tick_check_new_device(&mct_comp_device)에서 한일:
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

	clockevents_notify_released();

	raw_spin_unlock_irqrestore(&clockevents_lock, flags);

	// raw_spin_unlock_irqrestore에서 한일:
	// clockevents_lock를 사용하여 spin unlock을 수행하고 flags에 저장된 cpsr을 복원

	// raw_spin_unlock_irqrestore에서 한일:
	// clockevents_lock를 사용하여 spin unlock을 수행하고 flags에 저장된 cpsr을 복원
}
EXPORT_SYMBOL_GPL(clockevents_register_device);

// ARM10C 20150404
// dev: [pcp0] &(&percpu_mct_tick)->evt, freq: 12000000
// ARM10C 20150523
// dev: &mct_comp_device, freq: 24000000
void clockevents_config(struct clock_event_device *dev, u32 freq)
{
	u64 sec;

	// dev->features: [pcp0] (&(&percpu_mct_tick)->evt)->features: 0x3, CLOCK_EVT_FEAT_ONESHOT: 0x000002
	// dev->features: (&mct_comp_device)->features: 0x3, CLOCK_EVT_FEAT_ONESHOT: 0x000002
	if (!(dev->features & CLOCK_EVT_FEAT_ONESHOT))
		return;

	/*
	 * Calculate the maximum number of seconds we can sleep. Limit
	 * to 10 minutes for hardware which can program more than
	 * 32bit ticks so we still get reasonable conversion values.
	 */
	// dev->max_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ticks: 0x7fffffff
	// dev->max_delta_ticks: (&mct_comp_device)->max_delta_ticks: 0xffffffff
	sec = dev->max_delta_ticks;
	// sec: 0x7fffffff
	// sec: 0xffffffff

// 2015/04/04 종료
// 2015/04/11 시작

	// sec: 0x7fffffff, freq: 12000000
	// sec: 0xffffffff, freq: 24000000
	do_div(sec, freq);
	// sec: 178
	// sec: 178

	// sec: 178, dev->max_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ticks: 0x7fffffff,
	// UINT_MAX: 0xFFFFFFFF
	// sec: 178, dev->max_delta_ticks: [pcp0] (&mct_comp_device)->max_delta_ticks: 0xffffffff,
	// UINT_MAX: 0xFFFFFFFF
	if (!sec)
		sec = 1;
	else if (sec > 600 && dev->max_delta_ticks > UINT_MAX)
		sec = 600;

	// dev: [pcp0] &(&percpu_mct_tick)->evt, freq: 12000000, sec: 178
	// clockevents_calc_mult_shift([pcp0] &(&percpu_mct_tick)->evt, 12000000, 178)
	// dev: &mct_comp_device, freq: 24000000, sec: 178
	// clockevents_calc_mult_shift(&mct_comp_device, 24000000, 178)
	clockevents_calc_mult_shift(dev, freq, sec);

	// clockevents_calc_mult_shift에서 한일:
	// [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32

	// clockevents_calc_mult_shift에서 한일:
	// (&mct_comp_device)->mult: 0x3126E98
	// (&mct_comp_device)->shift: 31

	// dev->min_delta_ns: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ns,
	// dev->min_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ticks: 0xf, dev: [pcp0] &(&percpu_mct_tick)->evt
	// cev_delta2ns(0xf, [pcp0] &(&percpu_mct_tick)->evt, false): 0x4E2
	// dev->min_delta_ns: (&mct_comp_device)->min_delta_ns,
	// dev->min_delta_ticks: (&mct_comp_device)->min_delta_ticks: 0xf, dev: &mct_comp_device
	// cev_delta2ns(0xf, &mct_comp_device, false): 0x3E8
	dev->min_delta_ns = cev_delta2ns(dev->min_delta_ticks, dev, false);
	// dev->min_delta_ns: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ns: 0x4E2
	// dev->min_delta_ns: (&mct_comp_device)->min_delta_ns: 0x3E8

	// dev->max_delta_ns: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ns,
	// dev->max_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ticks: 0x7fffffff, dev: [pcp0] &(&percpu_mct_tick)->evt
	// cev_delta2ns(0x7fffffff, [pcp0] &(&percpu_mct_tick)->evt, true): 0x29AAAAA444
	//
	// dev->max_delta_ns: (&mct_comp_device)->max_delta_ns,
	// dev->max_delta_ticks: (&mct_comp_device)->max_delta_ticks: 0xffffffff, dev: &mct_comp_device
	// cev_delta2ns(0xffffffff, &mct_comp_device, true): 0x29AAAAA46E
	dev->max_delta_ns = cev_delta2ns(dev->max_delta_ticks, dev, true);
	// dev->max_delta_ns: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ns: 0x29AAAAA444
	// dev->max_delta_ns: (&mct_comp_device)->max_delta_ns: 0x29AAAAA46E
}

/**
 * clockevents_config_and_register - Configure and register a clock event device
 * @dev:	device to register
 * @freq:	The clock frequency
 * @min_delta:	The minimum clock ticks to program in oneshot mode
 * @max_delta:	The maximum clock ticks to program in oneshot mode
 *
 * min/max_delta can be 0 for devices which do not support oneshot mode.
 */
// ARM10C 20150404
// evt: [pcp0] &(&percpu_mct_tick)->evt, 12000000, 0xf, 0x7fffffff
// ARM10C 20150523
// &mct_comp_device, clk_rate: 24000000, 0xf, 0xffffffff
void clockevents_config_and_register(struct clock_event_device *dev,
				     u32 freq, unsigned long min_delta,
				     unsigned long max_delta)
{
	// dev->min_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ticks, min_delta: 0xf
	// dev->min_delta_ticks: (&mct_comp_device)->min_delta_ticks, min_delta: 0xf
	dev->min_delta_ticks = min_delta;
	// dev->min_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ticks: 0xf
	// dev->min_delta_ticks: (&mct_comp_device)->min_delta_ticks: 0xf

	// dev->max_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ticks, max_delta: 0x7fffffff
	// dev->max_delta_ticks: (&mct_comp_device)->max_delta_ticks, max_delta: 0xffffffff
	dev->max_delta_ticks = max_delta;
	// dev->max_delta_ticks: [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ticks: 0x7fffffff
	// dev->max_delta_ticks: (&mct_comp_device)->max_delta_ticks: 0xffffffff

	// dev: [pcp0] &(&percpu_mct_tick)->evt, freq: 12000000
	// clockevents_config(&(&percpu_mct_tick)->evt, 12000000)
	// dev: &mct_comp_device, freq: 24000000
	// clockevents_config(&mct_comp_device, 24000000)
	clockevents_config(dev, freq);

	// clockevents_config에서 한일:
	// [pcp0] (&(&percpu_mct_tick)->evt)->mult: 0x3126E98
	// [pcp0] (&(&percpu_mct_tick)->evt)->shift: 32
	// [pcp0] (&(&percpu_mct_tick)->evt)->min_delta_ns: 0x4E2
	// [pcp0] (&(&percpu_mct_tick)->evt)->max_delta_ns: 0x29AAAAA444

	// clockevents_config에서 한일:
	// (&mct_comp_device)->mult: 0x3126E98
	// (&mct_comp_device)->shift: 31
	// (&mct_comp_device)->min_delta_ns: 0x3E8
	// (&mct_comp_device)->max_delta_ns: 0x29AAAAA46E

	// dev: [pcp0] &(&percpu_mct_tick)->evt
	// dev: &mct_comp_device
	clockevents_register_device(dev);

	// clockevents_register_device([pcp0] &(&percpu_mct_tick)->evt)에서 한일:
	// list clockevent_devices에 [pcp0] (&(&percpu_mct_tick)->evt)->list를 추가함
	//
	// tick_do_timer_cpu: 0
	// tick_next_period.tv64: 0
	// tick_period.tv64: 10000000
	//
	// [pcp0] (&tick_cpu_device)->mode: 0
	// [pcp0] (&tick_cpu_device)->evtdev: [pcp0] &(&percpu_mct_tick)->evt
	//
	// [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x7FFFFFFFFFFFFFFF
	// [pcp0] (&(&percpu_mct_tick)->evt)->event_handler: tick_handle_periodic
	// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 2
	//
	// [pcp0] (&tick_cpu_sched)->check_clocks: 1
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

	// clockevents_register_device(&mct_comp_device)에서 한일:
	// list clockevent_devices에 (&mct_comp_device)->list를 추가함
	//
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
EXPORT_SYMBOL_GPL(clockevents_config_and_register);

/**
 * clockevents_update_freq - Update frequency and reprogram a clock event device.
 * @dev:	device to modify
 * @freq:	new device frequency
 *
 * Reconfigure and reprogram a clock event device in oneshot
 * mode. Must be called on the cpu for which the device delivers per
 * cpu timer events with interrupts disabled!  Returns 0 on success,
 * -ETIME when the event is in the past.
 */
int clockevents_update_freq(struct clock_event_device *dev, u32 freq)
{
	clockevents_config(dev, freq);

	if (dev->mode != CLOCK_EVT_MODE_ONESHOT)
		return 0;

	return clockevents_program_event(dev, dev->next_event, false);
}

/*
 * Noop handler when we shut down an event device
 */
void clockevents_handle_noop(struct clock_event_device *dev)
{
}

/**
 * clockevents_exchange_device - release and request clock devices
 * @old:	device to release (can be NULL)
 * @new:	device to request (can be NULL)
 *
 * Called from the notifier chain. clockevents_lock is held already
 */
// ARM10C 20150411
// curdev: [pcp0] (&tick_cpu_device)->evtdev: NULL, newdev: [pcp0] &(&percpu_mct_tick)->evt
// ARM10C 20150523
// cur: tick_broadcast_device.evtdev, dev: &mct_comp_device
void clockevents_exchange_device(struct clock_event_device *old,
				 struct clock_event_device *new)
{
	unsigned long flags;

	local_irq_save(flags);

	// local_irq_save 에서 한일:
	// cpsr을 flags에 저장하고 interrupt disable 함

	// local_irq_save 에서 한일:
	// cpsr을 flags에 저장하고 interrupt disable 함

	/*
	 * Caller releases a clock event device. We queue it into the
	 * released list and do a notify add later.
	 */
	// old: [pcp0] (&tick_cpu_device)->evtdev: NULL
	// old: tick_broadcast_device.evtdev: NULL
	if (old) {
		module_put(old->owner);
		clockevents_set_mode(old, CLOCK_EVT_MODE_UNUSED);
		list_del(&old->list);
		list_add(&old->list, &clockevents_released);
	}

	// new: [pcp0] &(&percpu_mct_tick)->evt: NULL 이 아닌 값
	// new: &mct_comp_device
	if (new) {
		// new->mode: [pcp0] (&(&percpu_mct_tick)->evt)->mode: 0, CLOCK_EVT_MODE_UNUSED: 0
		// new->mode: (&mct_comp_device)->mode: 0, CLOCK_EVT_MODE_UNUSED: 0
		BUG_ON(new->mode != CLOCK_EVT_MODE_UNUSED);

		// new: [pcp0] &(&percpu_mct_tick)->evt, clockevents_shutdown([pcp0] &(&percpu_mct_tick)->evt)
		// new: &mct_comp_device, clockevents_shutdown(&mct_comp_device)
		clockevents_shutdown(new);

		// clockevents_shutdown에서 한일:
		// timer control register L0_TCON 값을 읽어 timer start, timer interrupt 설정을
		// 동작하지 않도록 변경함
		// L0_TCON 값이 0 으로 가정하였으므로 timer는 동작하지 않은 상태임
		//
		// [pcp0] (&(&percpu_mct_tick)->evt)->mode: 1
		// [pcp0] (&(&percpu_mct_tick)->evt)->next_event.tv64: 0x7FFFFFFFFFFFFFFF

		// clockevents_shutdown에서 한일:
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
	}
	local_irq_restore(flags);

	// local_irq_restore 에서 한일:
	// flags에 저장된 cpsr을 복원하고 interrupt enable 함

	// local_irq_restore 에서 한일:
	// flags에 저장된 cpsr을 복원하고 interrupt enable 함
}

/**
 * clockevents_suspend - suspend clock devices
 */
void clockevents_suspend(void)
{
	struct clock_event_device *dev;

	list_for_each_entry_reverse(dev, &clockevent_devices, list)
		if (dev->suspend)
			dev->suspend(dev);
}

/**
 * clockevents_resume - resume clock devices
 */
void clockevents_resume(void)
{
	struct clock_event_device *dev;

	list_for_each_entry(dev, &clockevent_devices, list)
		if (dev->resume)
			dev->resume(dev);
}

#ifdef CONFIG_GENERIC_CLOCKEVENTS
/**
 * clockevents_notify - notification about relevant events
 */
void clockevents_notify(unsigned long reason, void *arg)
{
	struct clock_event_device *dev, *tmp;
	unsigned long flags;
	int cpu;

	raw_spin_lock_irqsave(&clockevents_lock, flags);

	switch (reason) {
	case CLOCK_EVT_NOTIFY_BROADCAST_ON:
	case CLOCK_EVT_NOTIFY_BROADCAST_OFF:
	case CLOCK_EVT_NOTIFY_BROADCAST_FORCE:
		tick_broadcast_on_off(reason, arg);
		break;

	case CLOCK_EVT_NOTIFY_BROADCAST_ENTER:
	case CLOCK_EVT_NOTIFY_BROADCAST_EXIT:
		tick_broadcast_oneshot_control(reason);
		break;

	case CLOCK_EVT_NOTIFY_CPU_DYING:
		tick_handover_do_timer(arg);
		break;

	case CLOCK_EVT_NOTIFY_SUSPEND:
		tick_suspend();
		tick_suspend_broadcast();
		break;

	case CLOCK_EVT_NOTIFY_RESUME:
		tick_resume();
		break;

	case CLOCK_EVT_NOTIFY_CPU_DEAD:
		tick_shutdown_broadcast_oneshot(arg);
		tick_shutdown_broadcast(arg);
		tick_shutdown(arg);
		/*
		 * Unregister the clock event devices which were
		 * released from the users in the notify chain.
		 */
		list_for_each_entry_safe(dev, tmp, &clockevents_released, list)
			list_del(&dev->list);
		/*
		 * Now check whether the CPU has left unused per cpu devices
		 */
		cpu = *((int *)arg);
		list_for_each_entry_safe(dev, tmp, &clockevent_devices, list) {
			if (cpumask_test_cpu(cpu, dev->cpumask) &&
			    cpumask_weight(dev->cpumask) == 1 &&
			    !tick_is_broadcast_device(dev)) {
				BUG_ON(dev->mode != CLOCK_EVT_MODE_UNUSED);
				list_del(&dev->list);
			}
		}
		break;
	default:
		break;
	}
	raw_spin_unlock_irqrestore(&clockevents_lock, flags);
}
EXPORT_SYMBOL_GPL(clockevents_notify);

#ifdef CONFIG_SYSFS
struct bus_type clockevents_subsys = {
	.name		= "clockevents",
	.dev_name       = "clockevent",
};

static DEFINE_PER_CPU(struct device, tick_percpu_dev);
static struct tick_device *tick_get_tick_dev(struct device *dev);

static ssize_t sysfs_show_current_tick_dev(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tick_device *td;
	ssize_t count = 0;

	raw_spin_lock_irq(&clockevents_lock);
	td = tick_get_tick_dev(dev);
	if (td && td->evtdev)
		count = snprintf(buf, PAGE_SIZE, "%s\n", td->evtdev->name);
	raw_spin_unlock_irq(&clockevents_lock);
	return count;
}
static DEVICE_ATTR(current_device, 0444, sysfs_show_current_tick_dev, NULL);

/* We don't support the abomination of removable broadcast devices */
static ssize_t sysfs_unbind_tick_dev(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	char name[CS_NAME_LEN];
	ssize_t ret = sysfs_get_uname(buf, name, count);
	struct clock_event_device *ce;

	if (ret < 0)
		return ret;

	ret = -ENODEV;
	mutex_lock(&clockevents_mutex);
	raw_spin_lock_irq(&clockevents_lock);
	list_for_each_entry(ce, &clockevent_devices, list) {
		if (!strcmp(ce->name, name)) {
			ret = __clockevents_try_unbind(ce, dev->id);
			break;
		}
	}
	raw_spin_unlock_irq(&clockevents_lock);
	/*
	 * We hold clockevents_mutex, so ce can't go away
	 */
	if (ret == -EAGAIN)
		ret = clockevents_unbind(ce, dev->id);
	mutex_unlock(&clockevents_mutex);
	return ret ? ret : count;
}
static DEVICE_ATTR(unbind_device, 0200, NULL, sysfs_unbind_tick_dev);

#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
static struct device tick_bc_dev = {
	.init_name	= "broadcast",
	.id		= 0,
	.bus		= &clockevents_subsys,
};

static struct tick_device *tick_get_tick_dev(struct device *dev)
{
	return dev == &tick_bc_dev ? tick_get_broadcast_device() :
		&per_cpu(tick_cpu_device, dev->id);
}

static __init int tick_broadcast_init_sysfs(void)
{
	int err = device_register(&tick_bc_dev);

	if (!err)
		err = device_create_file(&tick_bc_dev, &dev_attr_current_device);
	return err;
}
#else
static struct tick_device *tick_get_tick_dev(struct device *dev)
{
	return &per_cpu(tick_cpu_device, dev->id);
}
static inline int tick_broadcast_init_sysfs(void) { return 0; }
#endif

static int __init tick_init_sysfs(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct device *dev = &per_cpu(tick_percpu_dev, cpu);
		int err;

		dev->id = cpu;
		dev->bus = &clockevents_subsys;
		err = device_register(dev);
		if (!err)
			err = device_create_file(dev, &dev_attr_current_device);
		if (!err)
			err = device_create_file(dev, &dev_attr_unbind_device);
		if (err)
			return err;
	}
	return tick_broadcast_init_sysfs();
}

static int __init clockevents_init_sysfs(void)
{
	int err = subsys_system_register(&clockevents_subsys, NULL);

	if (!err)
		err = tick_init_sysfs();
	return err;
}
device_initcall(clockevents_init_sysfs);
#endif /* SYSFS */

#endif /* GENERIC_CLOCK_EVENTS */
