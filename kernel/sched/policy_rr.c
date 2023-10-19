/*
 * Copyright (c) 2022 Institute of Parallel And Distributed Systems (IPADS)
 * ChCore-Lab is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 */

/* Scheduler related functions are implemented here */
//#include <sched/sched.h>
//#include <arch/machine/smp.h>
//#include <common/kprint.h>
#include <machine.h>
//#include <mm/kmalloc.h>
//#include <common/list.h>
//#include <common/util.h>
//#include <object/thread.h>
//#include <common/macro.h>
//#include <common/errno.h>
//#include <common/types.h>
//#include <object/thread.h>
//#include <irq/irq.h>
//#include <sched/context.h>

#include "../include/sched/sched.h"
#include "../include/arch/aarch64/arch/machine/smp.h"
#include "../include/common/kprint.h"
#include "../include/mm/kmalloc.h"
#include "../include/common/list.h"
#include "../include/common/util.h"
#include "../include/object/thread.h"
#include "../include/common/macro.h"
#include "../include/common/errno.h"
#include "../include/common/types.h"
#include "../include/object/thread.h"
#include "../include/irq/irq.h"
#include "../include/sched/context.h"

/* in arch/sched/idle.S */
void idle_thread_routine(void);

/* Metadata for ready queue */
struct queue_meta {
        struct list_head queue_head;
        u32 queue_len;
        char pad[pad_to_cache_line(sizeof(u32) + sizeof(struct list_head))];
};

/*
 * rr_ready_queue
 * Per-CPU ready queue for ready tasks.
 */
struct queue_meta rr_ready_queue_meta[PLAT_CPU_NUM];

/*
 * RR policy also has idle threads.
 * When no active user threads in ready queue,
 * we will choose the idle thread to execute.
 * Idle thread will **NOT** be in the RQ.
 */
struct thread idle_threads[PLAT_CPU_NUM];

/*
 * Lab4
 * Sched_enqueue
 * Put `thread` at the end of ready queue of assigned `affinity`.
 * If affinity = NO_AFF, assign the core to the current cpu.
 * If the thread is IDLE thread, do nothing!
 * Do not forget to check if the affinity is valid!
 */
int rr_sched_enqueue(struct thread *thread)
{
        /* LAB 4 TODO BEGIN */
        if (!(thread && thread->thread_ctx
              && thread->thread_ctx->state != TS_READY)) {
                return -1;
        }
        if (thread->thread_ctx->type == TYPE_IDLE) {
                return 0;
        }
        s32 affinity = thread->thread_ctx->affinity;
        if (affinity == NO_AFF) {
                thread->thread_ctx->cpuid = smp_get_cpu_id();
        } else if (affinity >= 0 && affinity < PLAT_CPU_NUM) {
                thread->thread_ctx->cpuid = affinity;
        } else {
                return -1;
        }

        list_append(
                &thread->ready_queue_node,
                &(rr_ready_queue_meta[thread->thread_ctx->cpuid].queue_head));
        thread->thread_ctx->state = TS_READY;

        /* LAB 4 TODO END */
        return 0;
}

/*
 * Lab4
 * Sched_dequeue
 * remove `thread` from its current residual ready queue
 * Do not forget to add some basic parameter checking
 */
int rr_sched_dequeue(struct thread *thread)
{
        /* LAB 4 TODO BEGIN */
        if (thread == NULL || thread->thread_ctx == NULL
            || thread->thread_ctx->type == TYPE_IDLE
            || thread->thread_ctx->state != TS_READY
            || list_empty(
                    &thread->ready_queue_node)) // 不能对亲和性做检查，可能没设置
        {
                return -1;
        }
        thread->thread_ctx->state = TS_INTER;
        thread->thread_ctx->cpuid = smp_get_cpu_id(); // TODO: check
        list_del(&thread->ready_queue_node);
        /* LAB 4 TODO END */
        return 0;
}

/*
 * Lab4
 * Choose an appropriate thread and dequeue from ready queue
 *
 * Hints:
 * You can use `list_entry` to find the next ready thread in the ready queue
 */
struct thread *rr_sched_choose_thread(void)
{
        struct thread *thread = NULL;
        /* LAB 4 TODO BEGIN */
        int cpu_id = smp_get_cpu_id();
        struct list_head *head = &(rr_ready_queue_meta[cpu_id].queue_head);
        if (list_empty(head)) {
                thread = &idle_threads[cpu_id];
        } else {
                thread =
                        list_entry(head->next, struct thread, ready_queue_node);
                if (rr_sched_dequeue(thread) != 0) {
                        return NULL; // TODO: check
                }
        }
        /* LAB 4 TODO END */
        return thread;
}

/*
 * Lab4
 * You should use this function in rr_sched
 */
static inline void rr_sched_refill_budget(struct thread *target, u32 budget)
{
        /* LAB 4 TODO BEGIN */
        target->thread_ctx->sc->budget = budget;
        /* LAB 4 TODO END */
}

/*
 * Lab4
 * Schedule a thread to execute.
 * This function will suspend current running thread, if any, and schedule
 * another thread from `rr_ready_queue[cpu_id]`.
 *
 * Hints:
 * Macro DEFAULT_BUDGET defines the value for resetting thread's budget.
 * After you get one thread from rr_sched_choose_thread, pass it to
 * switch_to_thread() to prepare for switch_context().
 * Then ChCore can call eret_to_thread() to return to user mode.
 * You should also check the state of the old thread. Old thread
 * could be exiting/waiting or running when calling this function.
 * You will also need to check the remaining budget of the old thread.
 */
int rr_sched(void)
{
        /* LAB 4 TODO BEGIN */
        if (current_thread == NULL || current_thread->thread_ctx == NULL
            || current_thread->thread_ctx->sc == NULL) {
                struct thread *thread = rr_sched_choose_thread();
                switch_to_thread(thread);
                return 0;
        }

        if (current_thread->thread_ctx->sc->budget != 0) {
                return -1;
        }

        if (current_thread->thread_ctx->thread_exit_state == TE_EXITING) {
                current_thread->thread_ctx->thread_exit_state = TE_EXITED;
                current_thread->thread_ctx->state = TS_EXIT;
        }

        // waiting thread cannot enqueue
        if (current_thread->thread_ctx->type != TYPE_IDLE
            && current_thread->thread_ctx->state != TS_WAITING) {
                //              ret = sched_enqueue(current_thread);
                sched_enqueue(current_thread);
        }

        rr_sched_refill_budget(current_thread, DEFAULT_BUDGET);
        struct thread *thread = rr_sched_choose_thread();
        switch_to_thread(thread);
        /* LAB 4 TODO END */

        return 0;
}

int rr_sched_init(void)
{
        int i = 0;
        char idle_name[] = "KNL-IDLE-RR";
        int name_len = strlen(idle_name);

        struct cap_group *idle_cap_group;
        struct vmspace *idle_vmspace;

        /* Initialize global variables */
        for (i = 0; i < PLAT_CPU_NUM; i++) {
                current_threads[i] = NULL;
                init_list_head(&(rr_ready_queue_meta[i].queue_head));
                rr_ready_queue_meta[i].queue_len = 0;
        }

        /* Create a fake idle cap group to store the name */
        idle_cap_group = kzalloc(sizeof(*idle_cap_group));
        memset(idle_cap_group->cap_group_name, 0, MAX_GROUP_NAME_LEN);
        if (name_len > MAX_GROUP_NAME_LEN)
                name_len = MAX_GROUP_NAME_LEN;
        memcpy(idle_cap_group->cap_group_name, idle_name, name_len);
        init_list_head(&idle_cap_group->thread_list);

        extern struct vmspace *create_idle_vmspace(void);
        idle_vmspace = create_idle_vmspace();

        /* Initialize one idle thread for each core and insert into the RQ */
        for (i = 0; i < PLAT_CPU_NUM; i++) {
                /* Set the thread context of the idle threads */
                BUG_ON(!(idle_threads[i].thread_ctx =
                                 create_thread_ctx(TYPE_IDLE)));
                /* We will set the stack and func ptr in arch_idle_ctx_init */
                init_thread_ctx(&idle_threads[i], 0, 0, MIN_PRIO, TYPE_IDLE, i);
                /* Call arch-dependent function to fill the context of the idle
                 * threads */
                arch_idle_ctx_init(idle_threads[i].thread_ctx,
                                   idle_thread_routine);

                idle_threads[i].cap_group = idle_cap_group;
                idle_threads[i].vmspace = idle_vmspace;

                /* Add idle_threads to the threads list */
                list_add(&idle_threads[i].node, &idle_cap_group->thread_list);
        }
        kdebug("Scheduler initialized. Create %d idle threads.\n", i);

        return 0;
}

#define MAX_CAP_GROUP_BUF 256

void rr_top(void)
{
        u32 cpuid;
        struct thread *thread;
        /* A simple way to collect all cap groups */
        struct cap_group *cap_group_buf[MAX_CAP_GROUP_BUF] = {0};
        unsigned int cap_group_num = 0;
        int i = 0;

        printk("\n*****CPU RQ Info*****\n");
        for (cpuid = 0; cpuid < PLAT_CPU_NUM; cpuid++) {
                printk("== CPU %d RQ LEN %lu==\n",
                       cpuid,
                       rr_ready_queue_meta[cpuid].queue_len);
                thread = current_threads[cpuid];
                if (thread != NULL) {
                        for (i = 0; i < cap_group_num; i++)
                                if (thread->cap_group == cap_group_buf[i])
                                        break;
                        if (i == cap_group_num
                            && cap_group_num < MAX_CAP_GROUP_BUF) {
                                cap_group_buf[cap_group_num] =
                                        thread->cap_group;
                                cap_group_num++;
                        }
                        printk("Current ");
                        print_thread(thread);
                }
                if (!list_empty(&(rr_ready_queue_meta[cpuid].queue_head))) {
                        for_each_in_list (
                                thread,
                                struct thread,
                                ready_queue_node,
                                &(rr_ready_queue_meta[cpuid].queue_head)) {
                                for (i = 0; i < cap_group_num; i++)
                                        if (thread->cap_group
                                            == cap_group_buf[i])
                                                break;
                                if (i == cap_group_num
                                    && cap_group_num < MAX_CAP_GROUP_BUF) {
                                        cap_group_buf[cap_group_num] =
                                                thread->cap_group;
                                        cap_group_num++;
                                }
                                print_thread(thread);
                        }
                }
                printk("\n");
        }

        printk("\n*****CAP GROUP Info*****\n");
        for (i = 0; i < cap_group_num; i++) {
                printk("== CAP GROUP:%s ==\n",
                       cap_group_buf[i]->cap_group_name);
                for_each_in_list (thread,
                                  struct thread,
                                  node,
                                  &(cap_group_buf[i]->thread_list)) {
                        print_thread(thread);
                }
                printk("\n");
        }
}

struct sched_ops rr = {.sched_init = rr_sched_init,
                       .sched = rr_sched,
                       .sched_enqueue = rr_sched_enqueue,
                       .sched_dequeue = rr_sched_dequeue,
                       .sched_top = rr_top};