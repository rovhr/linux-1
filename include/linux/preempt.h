#ifndef __LINUX_PREEMPT_H
#define __LINUX_PREEMPT_H

/*
 * include/linux/preempt.h - macros for accessing and manipulating
 * preempt_count (used for kernel preemption, interrupt count, etc.)
 */

#include <linux/thread_info.h>
#include <linux/linkage.h>
#include <linux/list.h>

#if defined(CONFIG_DEBUG_PREEMPT) || defined(CONFIG_PREEMPT_TRACER)
  extern void add_preempt_count(int val);
  extern void sub_preempt_count(int val);
#else
# define add_preempt_count(val)	do { preempt_count() += (val); } while (0)
# define sub_preempt_count(val)	do { preempt_count() -= (val); } while (0)
#endif
/* preempt count를 1 증가 */
#define inc_preempt_count() add_preempt_count(1)
#define dec_preempt_count() sub_preempt_count(1)

/*! 20131221 현재 thread_info의 preempt_count 변수를 반환  */
#define preempt_count()	(current_thread_info()->preempt_count)

#ifdef CONFIG_PREEMPT

asmlinkage void preempt_schedule(void);

/*! 20131221 TIF_NEED_RESCHED 가 설정되어 있으면 해당함수 실행 */ 
#define preempt_check_resched() \
do { \
	if (unlikely(test_thread_flag(TIF_NEED_RESCHED))) \
		preempt_schedule(); \
} while (0)

#ifdef CONFIG_CONTEXT_TRACKING

void preempt_schedule_context(void);

#define preempt_check_resched_context() \
do { \
	if (unlikely(test_thread_flag(TIF_NEED_RESCHED))) \
		preempt_schedule_context(); \
} while (0)
#else

#define preempt_check_resched_context() preempt_check_resched()

#endif /* CONFIG_CONTEXT_TRACKING */

#else /* !CONFIG_PREEMPT */

#define preempt_check_resched()		do { } while (0)
#define preempt_check_resched_context()	do { } while (0)

#endif /* CONFIG_PREEMPT */


#ifdef CONFIG_PREEMPT_COUNT
/*! 20131221 preempt count를 증가시켜 disable 시킨다. */
/*! 20140104
 * http://nimhaplz.egloos.com/5683475 참고
 * 다른 thread가 동작하는 것을 막는다. 계속 현재 thread만 수행되도록 한다.
 * 인터럽트를 막지는 않는다.
 * preempt_count = 0 인 경우 preemption enable / 1 이상인 경우 preemption disable
 * 스케쥴링 나오면 자세히 봅시다.
 */
#define preempt_disable() \
do { \
	inc_preempt_count(); \
	barrier(); \
} while (0)

/*! 20131221 preempt count를 감소시켜 0이 되면 enable 된다.  */
#define sched_preempt_enable_no_resched() \
do { \
	barrier(); \
	dec_preempt_count(); \
} while (0)

/*! 20131221 preempt count를 감소한다. */
#define preempt_enable_no_resched()	sched_preempt_enable_no_resched()

/*! 20131221 여기가 실행된다. */ 
#define preempt_enable() \
do { \
	preempt_enable_no_resched(); \
	barrier(); \
	preempt_check_resched(); \
} while (0)

/* For debugging and tracer internals only! */
#define add_preempt_count_notrace(val)			\
	do { preempt_count() += (val); } while (0)
#define sub_preempt_count_notrace(val)			\
	do { preempt_count() -= (val); } while (0)
#define inc_preempt_count_notrace() add_preempt_count_notrace(1)
#define dec_preempt_count_notrace() sub_preempt_count_notrace(1)

#define preempt_disable_notrace() \
do { \
	inc_preempt_count_notrace(); \
	barrier(); \
} while (0)

#define preempt_enable_no_resched_notrace() \
do { \
	barrier(); \
	dec_preempt_count_notrace(); \
} while (0)

/* preempt_check_resched is OK to trace */
#define preempt_enable_notrace() \
do { \
	preempt_enable_no_resched_notrace(); \
	barrier(); \
	preempt_check_resched_context(); \
} while (0)

#else /* !CONFIG_PREEMPT_COUNT */

/*
 * Even if we don't have any preemption, we need preempt disable/enable
 * to be barriers, so that we don't have things like get_user/put_user
 * that can cause faults and scheduling migrate into our preempt-protected
 * region.
 */
#define preempt_disable()		barrier()
#define sched_preempt_enable_no_resched()	barrier()
#define preempt_enable_no_resched()	barrier()
#define preempt_enable()		barrier()

#define preempt_disable_notrace()		barrier()
#define preempt_enable_no_resched_notrace()	barrier()
#define preempt_enable_notrace()		barrier()

#endif /* CONFIG_PREEMPT_COUNT */

#ifdef CONFIG_PREEMPT_NOTIFIERS

struct preempt_notifier;

/**
 * preempt_ops - notifiers called when a task is preempted and rescheduled
 * @sched_in: we're about to be rescheduled:
 *    notifier: struct preempt_notifier for the task being scheduled
 *    cpu:  cpu we're scheduled on
 * @sched_out: we've just been preempted
 *    notifier: struct preempt_notifier for the task being preempted
 *    next: the task that's kicking us out
 *
 * Please note that sched_in and out are called under different
 * contexts.  sched_out is called with rq lock held and irq disabled
 * while sched_in is called without rq lock and irq enabled.  This
 * difference is intentional and depended upon by its users.
 */
struct preempt_ops {
	void (*sched_in)(struct preempt_notifier *notifier, int cpu);
	void (*sched_out)(struct preempt_notifier *notifier,
			  struct task_struct *next);
};

/**
 * preempt_notifier - key for installing preemption notifiers
 * @link: internal use
 * @ops: defines the notifier functions to be called
 *
 * Usually used in conjunction with container_of().
 */
struct preempt_notifier {
	struct hlist_node link;
	struct preempt_ops *ops;
};

void preempt_notifier_register(struct preempt_notifier *notifier);
void preempt_notifier_unregister(struct preempt_notifier *notifier);

static inline void preempt_notifier_init(struct preempt_notifier *notifier,
				     struct preempt_ops *ops)
{
	INIT_HLIST_NODE(&notifier->link);
	notifier->ops = ops;
}

#endif

#endif /* __LINUX_PREEMPT_H */
