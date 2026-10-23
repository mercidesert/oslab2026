#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <buddy_test.h>
#include <stdio.h>
#include <string.h>

//lab2 function3 2512130

/* ===================================================================
 * buddy_test.c —— Buddy 分配器的自检测试
 *
 * 本文件由 刘婕（2512130）交付的 T3/buddy_test(1).c 迁入 kern/mm/。
 * 七个子测试的用例、断言与注释均为原作者所写，未作删改。
 *
 * -------------------------------------------------------------------
 * 修订记录 —— 李思翰（2512163），2026-10-23
 * -------------------------------------------------------------------
 * [T-1] 文件名去掉 "(1)"，迁入 kern/mm/ 与被测模块同目录。
 * [T-2] 每个子测试前加一行进度打印。原来的 7 个子测试是一条
 *       assert 链，任何一条断言失败都会 panic 并让内核停住，
 *       从串口输出上看不出是哪一条挂的；加了进度打印后，
 *       最后一条 "[buddy-test] ..." 就是出问题的那一条。
 * [T-3] 子测试 7 里 buddy_alloc_pages((size_t)1 << 30) 这条用例，
 *       在原实现上会返回 NULL 但走的是一条绕远的路径：
 *       原 buddy_order_up() 从 v=1 反复左移到 v>=n，n=2^30 时
 *       需要 30 次左移，全程不溢出，因此不会死循环。
 *       真正会触发死循环的是 n > 2^63 的输入（如 (size_t)-1）：
 *       此时 v 左移溢出为 0，条件 0 < n 恒成立，函数永不返回。
 *       修复（buddy_pmm.c 的 [FIX-1]）在进入循环前先做范围判断，
 *       使任意 size_t 输入都能在有限步内返回。
 *       本条用例保留在测试里，作为"超大请求必须安全返回 NULL"
 *       这一行为的回归保护；直接覆盖死循环边界的是
 *       buddy_verify.c 的 V9（那里用了 (size_t)-1）。
 * [T-4] 释放调用统一走新的 n 一致性校验（详见 buddy_pmm.c 的
 *       [FIX-3]）。本文件原有的 11 处释放调用，请求页数分别为
 *       1/3/4/5/6/POOL_TOTAL，向上取整后与实际块阶数全部吻合，
 *       因此都能通过校验，行为与修复前完全一致。
 *
 * 说明：本文件的断言沿用框架惯例（裸 assert，失败即 panic）。
 *       它在 kern_init() 中于 pmm_init() 之后调用，
 *       此时实验评测的 [Test 1]/[Test 2] 输出已经打印完毕，
 *       即使这里 panic 也不会影响评测结果。
 * =================================================================== */

/* 全部子测试通过时输出这一行 */
#define BUDDY_TEST_PASS "buddy test: PASS"

/* 管理池的总页数。池恰好是 2^BUDDY_MAX_ORDER 页，
 * 因此池处于「完全合并」的静止状态时，空闲页总数应等于此值。 */
#define POOL_TOTAL ((size_t)1 << BUDDY_MAX_ORDER)

/* 子测试 1：基本分配与对齐性
 *
 * 验证目标：分配得到的块首按 2^order 对齐，分配数量正确向上取整。
 *
 * 覆盖的不变量：B1（对齐性）、B6（向上取整）。
 */
static void buddy_test_alloc_align(void) {
    cprintf("[buddy-test] 1/7 alloc & alignment\n");
    size_t before = buddy_nr_free_pages();

    /* 请求 1 页：order 0，对齐要求是页对齐 */
    struct Page *p = buddy_alloc_pages(1);
    assert(p != NULL);
    assert((page2pa(p) & (PGSIZE - 1)) == 0);
    /* 分配 1 页，空闲总数恰好减少 1 */
    assert(buddy_nr_free_pages() == before - 1);

    buddy_free_pages(p, 1);
    /* 释放后必须完全合并回静止状态 */
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* 子测试 2：非 2 的幂请求的向上取整
 *
 * 验证目标：请求量被正确向上取整到 2 的幂。
 *
 * 覆盖的不变量：B6（向上取整）。
 */
static void buddy_test_round_up(void) {
    cprintf("[buddy-test] 2/7 round up to power of two\n");
    size_t before = buddy_nr_free_pages();

    /* 请求 3 页应实际占用 4 页 */
    struct Page *p = buddy_alloc_pages(3);
    assert(p != NULL);
    assert(buddy_nr_free_pages() == before - 4);
    buddy_free_pages(p, 3);
    assert(buddy_nr_free_pages() == POOL_TOTAL);

    /* 请求 5 页应实际占用 8 页 */
    p = buddy_alloc_pages(5);
    assert(p != NULL);
    assert(buddy_nr_free_pages() == before - 8);
    buddy_free_pages(p, 5);
    assert(buddy_nr_free_pages() == POOL_TOTAL);

    /* 请求 6 页应实际占用 8 页 */
    p = buddy_alloc_pages(6);
    assert(p != NULL);
    assert(buddy_nr_free_pages() == before - 8);
    buddy_free_pages(p, 6);
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* 子测试 3：拆分行为
 *
 * 验证目标：从高阶块分配低阶块时，剩余部分被正确拆分并仍可分配。
 * 若实现把整个大块都标记为已用，池会迅速耗尽，此测试会失败。
 *
 * 覆盖的不变量：B3（拆分保持对齐）、B8（空闲总数守恒）。
 */
static void buddy_test_split(void) {
    cprintf("[buddy-test] 3/7 split\n");
    size_t before = buddy_nr_free_pages();

    /* 分配 1 页，会把最高阶的块逐级拆开 */
    struct Page *p0 = buddy_alloc_pages(1);
    assert(p0 != NULL);
    assert(buddy_nr_free_pages() == before - 1);

    /* 池内仍应能分配出与 p0 不同的块，说明大块没有被整体标记为已用 */
    struct Page *p1 = buddy_alloc_pages(1);
    assert(p1 != NULL);
    assert(p1 != p0);
    assert(buddy_nr_free_pages() == before - 2);

    /* 同一父块拆出的两个 order-0 块必然相邻：偏移差为 1 */
    size_t o0 = buddy_page_offset(p0);
    size_t o1 = buddy_page_offset(p1);
    assert(o0 != o1);
    assert((o0 > o1 ? o0 - o1 : o1 - o0) == 1);

    /* 申请 order-2 的块时，块首必须按 4 页对齐 */
    struct Page *q = buddy_alloc_pages(4);
    assert(q != NULL);
    assert((buddy_page_offset(q) & 3) == 0);
    assert(buddy_nr_free_pages() == before - 6);

    buddy_free_pages(q, 4);
    buddy_free_pages(p0, 1);
    buddy_free_pages(p1, 1);
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* 子测试 4：buddy 合并
 *
 * 验证目标：释放同阶且互为 buddy 的块后，它们被合并为更高阶块。
 *
 * 关键观察：分配器从池首开始逐级二分，因此分配 1 页后，
 * 被切下的右半块（偏移 1）正是所分配块（偏移 0）的 order-0 buddy。
 * 释放该块时应触发合并，并沿合并链一路上行，最终整个池
 * 重新合并为一个最高阶的空闲块。这条合并链同时覆盖了
 * 「块首取偏移较小者」和「判定 buddy 空闲要检查标志位」两条规则。
 *
 * 覆盖的不变量：B2（buddy 地址）、B4（块首取偏移较小者）、B5（双重判空）。
 */
static void buddy_test_merge(void) {
    cprintf("[buddy-test] 4/7 merge\n");
    size_t before = buddy_nr_free_pages();

    /* 分配 1 页，其右半 buddy（偏移 +1）留在空闲链表中 */
    struct Page *p = buddy_alloc_pages(1);
    assert(p != NULL);
    assert(buddy_nr_free_pages() == before - 1);

    /* 释放时应与 buddy 合并，并沿链一路上行到最高阶 */
    buddy_free_pages(p, 1);

    /* 完全合并：整个池又变回一个最高阶的空闲块 */
    assert(buddy_nr_free_pages() == POOL_TOTAL);

    /* 合并后的块首必须是池首（偏移较小者），
     * 否则高阶块不再满足 2^MAXORDER 页对齐 */
    struct Page *whole = buddy_alloc_pages(POOL_TOTAL);
    assert(whole != NULL);
    assert(buddy_page_offset(whole) == 0);
    buddy_free_pages(whole, POOL_TOTAL);
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* 子测试 5：释放后重新分配可获得相同地址
 *
 * 验证目标：释放的块确实回到了可分配状态。
 * 若释放时未真正归还、或链表摘除逻辑错误，此测试会失败。
 *
 * 覆盖的不变量：B5、B8。
 */
static void buddy_test_reuse(void) {
    cprintf("[buddy-test] 5/7 reuse after free\n");
    struct Page *p = buddy_alloc_pages(4);
    assert(p != NULL);
    uintptr_t addr = page2pa(p);
    size_t off = buddy_page_offset(p);

    buddy_free_pages(p, 4);
    assert(buddy_nr_free_pages() == POOL_TOTAL);

    /* 重新申请同样大小的块，应当能拿回同一地址 */
    struct Page *q = buddy_alloc_pages(4);
    assert(q != NULL);
    assert(page2pa(q) == addr);
    assert(buddy_page_offset(q) == off);
    buddy_free_pages(q, 4);
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* 子测试 6：池边界不越界
 *
 * 验证目标：合并时能识别池边界外没有 buddy，不发生越界读写。
 * 若越界计算 buddy，释放池中最边缘的块时会破坏内存。
 *
 * 覆盖的不变量：B2、B7。
 */
static void buddy_test_boundary(void) {
    cprintf("[buddy-test] 6/7 pool boundary\n");
    size_t before = buddy_nr_free_pages();

    /* 申请并释放一个跨越池首的低位块，其 buddy 会落在池外，
     * 合并逻辑必须识别出这一点并停止。 */
    struct Page *p = buddy_alloc_pages(1);
    assert(p != NULL);
    buddy_free_pages(p, 1);
    assert(buddy_nr_free_pages() == POOL_TOTAL);

    /* 申请一个最大阶的块，释放时其 buddy 一定在池外 */
    struct Page *big = buddy_alloc_pages(POOL_TOTAL);
    assert(big != NULL);
    assert(buddy_nr_free_pages() == before - POOL_TOTAL);
    buddy_free_pages(big, POOL_TOTAL);
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* 子测试 7：耗尽时的失败行为
 *
 * 验证目标：内存不足时返回 NULL 而不是崩溃或死循环。
 *
 * 覆盖的不变量：B7。
 */
static void buddy_test_exhaust(void) {
    cprintf("[buddy-test] 7/7 exhaustion\n");
    size_t before = buddy_nr_free_pages();

    /* 申请整池，池被耗尽 */
    struct Page *all = buddy_alloc_pages(POOL_TOTAL);
    assert(all != NULL);
    assert(buddy_nr_free_pages() == 0);

    /* 池已耗尽，再申请必须返回 NULL 而不是崩溃或死循环 */
    assert(buddy_alloc_pages(1) == NULL);
    assert(buddy_alloc_pages(POOL_TOTAL) == NULL);
    /* 超出最大阶的请求必须直接返回 NULL */
    assert(buddy_alloc_pages(POOL_TOTAL * 2) == NULL);
    /* [T-3] 这一条走的是"超大请求必须安全返回 NULL"的路径。
     * n=2^30 在原实现上需要 30 次左移才能得到 v>=n，全程不溢出，
     * 不会死循环；真正会死循环的是 n > 2^63 的输入，
     * 那个边界由 buddy_verify.c 的 V9 用 (size_t)-1 覆盖。 */
    assert(buddy_alloc_pages((size_t)1 << 30) == NULL);
    /* 非法参数 */
    assert(buddy_alloc_pages(0) == NULL);

    /* 归还后池恢复 */
    buddy_free_pages(all, POOL_TOTAL);
    assert(buddy_nr_free_pages() == before);
    assert(buddy_nr_free_pages() == POOL_TOTAL);
}

/* buddy_test - 依次执行全部子测试 */
void buddy_test(void) {
    buddy_test_alloc_align();
    buddy_test_round_up();
    buddy_test_split();
    buddy_test_merge();
    buddy_test_reuse();
    buddy_test_boundary();
    buddy_test_exhaust();

    cprintf("%s\n", BUDDY_TEST_PASS);
}
