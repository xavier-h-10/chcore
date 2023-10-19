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

//#include <semaphore/semaphore.h>
//#include <sched/sched.h>
//#include <object/thread.h>
//#include <sched/context.h>
//#include <irq/irq.h>

#include "../include/semaphore/semaphore.h"
#include "../include/sched/sched.h"
#include "../include/object/thread.h"
#include "../include/sched/context.h"
#include "../include/irq/irq.h"

void init_sem(struct semaphore *sem)
{
        sem->sem_count = 0;
        sem->waiting_threads_count = 0;
        init_list_head(&sem->waiting_threads);
}

/*
 * Lab4
 * Return 0 if wait successfully, -EAGAIN otherwise
 * Hint: use sched() and eret_to_thread if current thread should be blocked.
 * You should also update the state of current thread to TS_WAITING
 * and set the return value of the current thread.
 * Besides, you should `obj_put` the sem before eret to the new thread.
 */
s32 wait_sem(struct semaphore *sem, bool is_block)
{
        s32 ret = 0;
        /* LAB 4 TODO BEGIN */
        if (sem->sem_count > 0) {
                sem->sem_count--;
        } else {
                if (!is_block) {
                        return -EAGAIN;
                }
                sem->waiting_threads_count++;
                list_append(&(current_thread->sem_queue_node),
                            &(sem->waiting_threads));
                current_thread->thread_ctx->state = TS_WAITING;
                //  arch_set_thread_return(current_thread,-EAGAIN);
                arch_set_thread_return(current_thread, -EAGAIN);
                obj_put(sem);
                if (current_thread && current_thread->thread_ctx
                    && current_thread->thread_ctx->sc) {
                        current_thread->thread_ctx->sc->budget = 0;
                }
                sched();
                eret_to_thread(switch_context());
        }
        /* LAB 4 TODO END */

        return ret;
}

/*
 * Lab4
 * Return 0 if signal successfully
 * Hint: use `list_entry` to find the wakeup target in the waiting_threads list
 * Remember to delete the thread from the list using `list_del`.
 * Enqueue it to the ready queue rather than directly switch to it.
 */
s32 signal_sem(struct semaphore *sem)
{
        /* LAB 4 TODO BEGIN */
        sem->sem_count++;
        struct list_head *head = &(sem->waiting_threads);
        if (sem->waiting_threads_count > 0) {
                // struct list_head *head = &(sem->waiting_threads);
                struct thread *thread =
                        list_entry(head->next, struct thread, sem_queue_node);
                sem->waiting_threads_count--;
                //                list_del((struct list_head *)thread);
                list_del(head->next);
                //                thread->thread_ctx->state = TS_READY;
                sched_enqueue(thread);
        }
        /* LAB 4 TODO END */
        return 0;
}

s32 sys_create_sem(void)
{
        struct semaphore *sem = NULL;
        int sem_cap = 0;
        int ret = 0;

        sem = obj_alloc(TYPE_SEMAPHORE, sizeof(*sem));
        if (!sem) {
                ret = -ENOMEM;
                goto out_fail;
        }
        init_sem(sem);

        sem_cap = cap_alloc(current_cap_group, sem, 0);
        if (sem_cap < 0) {
                ret = sem_cap;
                goto out_free_obj;
        }

        return sem_cap;
out_free_obj:
        obj_free(sem);
out_fail:
        return ret;
}

s32 sys_wait_sem(u32 sem_cap, bool is_block)
{
        struct semaphore *sem = NULL;
        int ret;

        sem = obj_get(current_thread->cap_group, sem_cap, TYPE_SEMAPHORE);
        if (!sem) {
                ret = -ECAPBILITY;
                goto out;
        }
        ret = wait_sem(sem, is_block);
        obj_put(sem);
out:
        return ret;
}

s32 sys_signal_sem(u32 sem_cap)
{
        struct semaphore *sem = NULL;
        int ret;
        sem = obj_get(current_thread->cap_group, sem_cap, TYPE_SEMAPHORE);
        if (!sem) {
                ret = -ECAPBILITY;
                goto out;
        }
        ret = signal_sem(sem);
        obj_put(sem);
out:
        return ret;
}
