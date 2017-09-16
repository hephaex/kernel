/*
 * Generic wait-for-completion handler;
 *
 * It differs from semaphores in that their default case is the opposite,
 * wait_for_completion default blocks whereas semaphore default non-block. The
 * interface also makes it easy to 'complete' multiple waiting threads,
 * something which isn't entirely natural for semaphores.
 *
 * But more importantly, the primitive documents the usage. Semaphores would
 * typically be used for exclusion which gives rise to priority inversion.
 * Waiting for completion is a typically sync point, but not an exclusion point.
 */

#include <linux/sched.h>
#include <linux/completion.h>

/**
 * complete: - signals a single thread waiting on this completion
 * @x:  holds the state of this particular completion
 *
 * This will wake up a single thread waiting on this completion. Threads will be
 * awakened in the same order in which they were queued.
 *
 * See also complete_all(), wait_for_completion() and related routines.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
// ARM10C 20170701
// &kthreadd_done
void complete(struct completion *x)
{
	unsigned long flags;

	// &x->wait.lock: &(&kthreadd_done)->wait.lock
	spin_lock_irqsave(&x->wait.lock, flags);

	// spin_lock_irqsave 에서 한일:
	// &(&kthreadd_done)->wait.lock 을 이용하여 spin lock 을 수행하고 cpsr을 flags에 저장

	// x->done: (&kthreadd_done)->done: 0
	x->done++;
	// x->done: (&kthreadd_done)->done: 1

	// &x->wait: &(&kthreadd_done)->wait, TASK_NORMAL: 3
	__wake_up_locked(&x->wait, TASK_NORMAL, 1);
	spin_unlock_irqrestore(&x->wait.lock, flags);

	// spin_unlock_irqrestore 에서 한일
	// &(&kthreadd_done)->wait.lock 을 이용하여 spin unlock 을 수행하고 flags에 저장된 cpsr을 복원
}
EXPORT_SYMBOL(complete);

/**
 * complete_all: - signals all threads waiting on this completion
 * @x:  holds the state of this particular completion
 *
 * This will wake up all threads waiting on this particular completion event.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void complete_all(struct completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->wait.lock, flags);
	x->done += UINT_MAX/2;
	__wake_up_locked(&x->wait, TASK_NORMAL, 0);
	spin_unlock_irqrestore(&x->wait.lock, flags);
}
EXPORT_SYMBOL(complete_all);

// ARM10C 20170830
// x: &kthreadd_done, action: schedule_timeout, timeout: 0x7FFFFFFF, state: 2
static inline long __sched
do_wait_for_common(struct completion *x,
		   long (*action)(long), long timeout, int state)
{
	// x->done: (&kthreadd_done)->done: 0
	if (!x->done) {
		// current: kmem_cache#15-oX (struct task_struct) (pid: 1)
		DECLARE_WAITQUEUE(wait, current);
		// DECLARE_WAITQUEUE(wait, kmem_cache#15-oX (struct task_struct) (pid: 1)):
		// wait_queue_t wait =
		// {
		//     .private	= kmem_cache#15-oX (struct task_struct) (pid: 1),
		//     .func = default_wake_function,
		//     .task_list = { NULL, NULL }
		// }

		// &x->wait: &(&kthreadd_done)->wait
		__add_wait_queue_tail_exclusive(&x->wait, &wait);

		// __add_wait_queue_tail_exclusive 에서 한일:
		// (&wait)->flags: ? | 0x01
		// &(&(&kthreadd_done)->wait)->task_list 과 (&(&(&kthreadd_done)->wait)->task_list)->prev 사이에 &(&wait)->task_list 를 추가함
		// 간단히 말하면 head 인 &(&(&kthreadd_done)->wait)->task_list 의 tail에 &(&wait)->task_list 가 추가됨
		//
		// (&(&(&kthreadd_done)->wait)->task_list)->prev = &(&wait)->task_list;
		// (&(&wait)->task_list)->next = &(&(&kthreadd_done)->wait)->task_list;
		// (&(&wait)->task_list)->prev = &(&(&kthreadd_done)->wait)->task_list;
		// (&(&(&kthreadd_done)->wait)->task_list)->next = &(&wait)->task_list;

// 2017/08/30 종료
// 2017/09/06 시작

		do {
			// state: 2, current: kmem_cache#15-oX (struct task_struct) (pid: 1)
			// signal_pending_state(2, kmem_cache#15-oX (struct task_struct) (pid: 1)): 0
			if (signal_pending_state(state, current)) {
				timeout = -ERESTARTSYS;
				break;
			}

			// state: 2
			__set_current_state(state);

			// __set_current_state 에서 한일:
			// (kmem_cache#15-oX (struct task_struct) (pid: 1))->state: 2

			// &x->wait.lock: &(&kthreadd_done)->wait.lock
			spin_unlock_irq(&x->wait.lock);

			// spin_unlock_irq 에서 한일:
			// &(&kthreadd_done)->wait.lock 을 사용하여 spin unlock 을 수행

			// action: schedule_timeout, timeout: 0x7FFFFFFF
			timeout = action(timeout);
			spin_lock_irq(&x->wait.lock);
		} while (!x->done && timeout);
		__remove_wait_queue(&x->wait, &wait);
		if (!x->done)
			return timeout;
	}
	x->done--;
	return timeout ?: 1;
}

// ARM10C 20170830
// x: &kthreadd_done, schedule_timeout, timeout: 0x7FFFFFFF, state: 2
static inline long __sched
__wait_for_common(struct completion *x,
		  long (*action)(long), long timeout, int state)
{
	might_sleep(); // null function

	// &x->wait.lock: &(&kthreadd_done)->wait.lock
	spin_lock_irq(&x->wait.lock);

	// spin_lock_irq 에서 한일:
	// &(&kthreadd_done)->wait.lock 을 사용하여 spin lock 수행

	// x: &kthreadd_done, action: schedule_timeout, timeout: 0x7FFFFFFF, state: 2
	timeout = do_wait_for_common(x, action, timeout, state);
	spin_unlock_irq(&x->wait.lock);
	return timeout;
}

// ARM10C 20170830
// x: &kthreadd_done, MAX_SCHEDULE_TIMEOUT: 0x7FFFFFFF, TASK_UNINTERRUPTIBLE: 2
static long __sched
wait_for_common(struct completion *x, long timeout, int state)
{
	// x: &kthreadd_done, timeout: 0x7FFFFFFF, state: 2
	return __wait_for_common(x, schedule_timeout, timeout, state);
}

static long __sched
wait_for_common_io(struct completion *x, long timeout, int state)
{
	return __wait_for_common(x, io_schedule_timeout, timeout, state);
}

/**
 * wait_for_completion: - waits for completion of a task
 * @x:  holds the state of this particular completion
 *
 * This waits to be signaled for completion of a specific task. It is NOT
 * interruptible and there is no timeout.
 *
 * See also similar routines (i.e. wait_for_completion_timeout()) with timeout
 * and interrupt capability. Also see complete().
 */
// ARM10C 20170830
// &kthreadd_done
void __sched wait_for_completion(struct completion *x)
{
	// x: &kthreadd_done, MAX_SCHEDULE_TIMEOUT: 0x7FFFFFFF, TASK_UNINTERRUPTIBLE: 2
	wait_for_common(x, MAX_SCHEDULE_TIMEOUT, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion);

/**
 * wait_for_completion_timeout: - waits for completion of a task (w/timeout)
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. The timeout is in jiffies. It is not
 * interruptible.
 *
 * Return: 0 if timed out, and positive (at least 1, or number of jiffies left
 * till timeout) if completed.
 */
unsigned long __sched
wait_for_completion_timeout(struct completion *x, unsigned long timeout)
{
	return wait_for_common(x, timeout, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_timeout);

/**
 * wait_for_completion_io: - waits for completion of a task
 * @x:  holds the state of this particular completion
 *
 * This waits to be signaled for completion of a specific task. It is NOT
 * interruptible and there is no timeout. The caller is accounted as waiting
 * for IO.
 */
void __sched wait_for_completion_io(struct completion *x)
{
	wait_for_common_io(x, MAX_SCHEDULE_TIMEOUT, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_io);

/**
 * wait_for_completion_io_timeout: - waits for completion of a task (w/timeout)
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. The timeout is in jiffies. It is not
 * interruptible. The caller is accounted as waiting for IO.
 *
 * Return: 0 if timed out, and positive (at least 1, or number of jiffies left
 * till timeout) if completed.
 */
unsigned long __sched
wait_for_completion_io_timeout(struct completion *x, unsigned long timeout)
{
	return wait_for_common_io(x, timeout, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_io_timeout);

/**
 * wait_for_completion_interruptible: - waits for completion of a task (w/intr)
 * @x:  holds the state of this particular completion
 *
 * This waits for completion of a specific task to be signaled. It is
 * interruptible.
 *
 * Return: -ERESTARTSYS if interrupted, 0 if completed.
 */
int __sched wait_for_completion_interruptible(struct completion *x)
{
	long t = wait_for_common(x, MAX_SCHEDULE_TIMEOUT, TASK_INTERRUPTIBLE);
	if (t == -ERESTARTSYS)
		return t;
	return 0;
}
EXPORT_SYMBOL(wait_for_completion_interruptible);

/**
 * wait_for_completion_interruptible_timeout: - waits for completion (w/(to,intr))
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. It is interruptible. The timeout is in jiffies.
 *
 * Return: -ERESTARTSYS if interrupted, 0 if timed out, positive (at least 1,
 * or number of jiffies left till timeout) if completed.
 */
long __sched
wait_for_completion_interruptible_timeout(struct completion *x,
					  unsigned long timeout)
{
	return wait_for_common(x, timeout, TASK_INTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_interruptible_timeout);

/**
 * wait_for_completion_killable: - waits for completion of a task (killable)
 * @x:  holds the state of this particular completion
 *
 * This waits to be signaled for completion of a specific task. It can be
 * interrupted by a kill signal.
 *
 * Return: -ERESTARTSYS if interrupted, 0 if completed.
 */
int __sched wait_for_completion_killable(struct completion *x)
{
	long t = wait_for_common(x, MAX_SCHEDULE_TIMEOUT, TASK_KILLABLE);
	if (t == -ERESTARTSYS)
		return t;
	return 0;
}
EXPORT_SYMBOL(wait_for_completion_killable);

/**
 * wait_for_completion_killable_timeout: - waits for completion of a task (w/(to,killable))
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be
 * signaled or for a specified timeout to expire. It can be
 * interrupted by a kill signal. The timeout is in jiffies.
 *
 * Return: -ERESTARTSYS if interrupted, 0 if timed out, positive (at least 1,
 * or number of jiffies left till timeout) if completed.
 */
long __sched
wait_for_completion_killable_timeout(struct completion *x,
				     unsigned long timeout)
{
	return wait_for_common(x, timeout, TASK_KILLABLE);
}
EXPORT_SYMBOL(wait_for_completion_killable_timeout);

/**
 *	try_wait_for_completion - try to decrement a completion without blocking
 *	@x:	completion structure
 *
 *	Return: 0 if a decrement cannot be done without blocking
 *		 1 if a decrement succeeded.
 *
 *	If a completion is being used as a counting completion,
 *	attempt to decrement the counter without blocking. This
 *	enables us to avoid waiting if the resource the completion
 *	is protecting is not available.
 */
bool try_wait_for_completion(struct completion *x)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&x->wait.lock, flags);
	if (!x->done)
		ret = 0;
	else
		x->done--;
	spin_unlock_irqrestore(&x->wait.lock, flags);
	return ret;
}
EXPORT_SYMBOL(try_wait_for_completion);

/**
 *	completion_done - Test to see if a completion has any waiters
 *	@x:	completion structure
 *
 *	Return: 0 if there are waiters (wait_for_completion() in progress)
 *		 1 if there are no waiters.
 *
 */
bool completion_done(struct completion *x)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&x->wait.lock, flags);
	if (!x->done)
		ret = 0;
	spin_unlock_irqrestore(&x->wait.lock, flags);
	return ret;
}
EXPORT_SYMBOL(completion_done);
