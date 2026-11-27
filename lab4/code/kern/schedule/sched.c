#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2512130

/* schedule - 在 proc_list 上环形扫描，挑一个 PROC_RUNNABLE 的线程交给 proc_run。
 *
 * 几个细节：
 *   1. 起点是"current 的下一个"，而不是表头。这样多个同优先级线程轮转时
 *      不会出现某个线程永远抢不到 CPU；current 是 idleproc 时从表头开始。
 *   2. 扫描用 do-while 环形走一圈，遇到第一个 RUNNABLE 就停；走完一圈都没
 *      找到，则回落到 idleproc（它永远是 RUNNABLE，是最后的兜底）。
 *   3. 先清 current->need_resched，再切换。若清在切换之后，被切走的线程
 *      重新回来时会带着上一轮的置位，导致不必要的重复调度。
 *   4. 整个过程关中断：proc_list 是全局链表，若扫描到一半被时钟中断打断，
 *      中断处理里又调 schedule 就会破坏链表遍历。
 */
void schedule(void)
{
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;

    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        do
        {
            if ((le = list_next(le)) != &proc_list)
            {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE)
                {
                    break;
                }
            }
        } while (le != last);

        if (next == NULL || next->state != PROC_RUNNABLE)
        {
            next = idleproc;
        }
        next->runs++;
        if (next != current)
        {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}



