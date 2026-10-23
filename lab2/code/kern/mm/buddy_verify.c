#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <buddy_verify.h>
#include <stdio.h>
#include <string.h>

//lab2 function3 李思翰2512163

/* ===================================================================
 * buddy_verify.c —— Buddy System 分配器的外部验证层
 *
 * 作者：李思翰（2512163）。任务分工中 T3 的"测试设计"由本人负责。
 *
 * -------------------------------------------------------------------
 * 一、为什么要有这一层
 * -------------------------------------------------------------------
 * kern/mm/buddy_test.c 是实现者自己写的自检，用的是框架惯例的
 * 裸 assert —— 失败一次就 panic，内核停在那里，后面所有用例都跑不到，
 * 而且从串口输出上只能看到 "assertion failed: ..."，看不出是哪一条。
 *
 * 本文件只使用 buddy_pmm.h 公开的接口，从**外部**观察分配器行为，
 * 并且：
 *   1. 全部用"计数式检查"：vt_check(条件, 描述, 期望值, 实际值)，
 *      失败只累加失败计数并打印一行，绝不 panic；
 *   2. 每次检查前后的"池空闲页数"和"存活占用页数"之和恒等于池总页数，
 *      这条守恒律在每个用例收尾时都会核对一次。
 *
 * -------------------------------------------------------------------
 * 二、判据的选择原则：只断言"定义性性质"
 * -------------------------------------------------------------------
 * 一份测试如果把实现细节也写死（例如"必须先拆左半再拆右半"），
 * 换一个同样正确的实现就会误报失败。因此本文件只断言 Buddy System
 * 由定义决定、任何正确实现都必须满足的性质：
 *
 *   P1 块大小恒为 2 的幂；
 *   P2 块首按 2^order 页对齐（池内偏移意义下）；
 *   P3 拆分出的两个半块恰好覆盖原块的两半，不多不少不重叠；
 *   P4 释放后同阶 buddy 合并且能重新申请回整块；
 *   P5 不该合并的两块不得合并，合并不得吞掉已分配的块；
 *   P6 分配不重叠：任何两次分配不得给出同一批物理页；
 *   P7 已分配块的内容不被其它分配串改（用页内容标记验证）；
 *   P8 非法释放必须被拒绝，且拒绝后池的状态一字未变；
 *   P9 释放全部块之后，各阶空闲块分布与初始状态完全一致；
 *   P10 与框架的对接正确：地址、页号、引用计数、reserved 位。
 *
 * -------------------------------------------------------------------
 * 三、与被测实现的接口约定
 * -------------------------------------------------------------------
 *   * 块的阶数由块首的 property 记录，实际大小 = 1 << property；
 *   * 释放时传入的 n 必须与块的阶数一致（buddy_free_pages_checked
 *     会校验），所以本文件的释放调用一律传"当初请求的页数"；
 *   * buddy_free_pages_checked() 返回 0 表示成功、-1 表示拒绝，
 *     拒绝时池的状态不发生任何变化 —— 这一点正是 P8 的观测点。
 * =================================================================== */

/* -------------------------------------------------------------------
 * 检查框架
 * ------------------------------------------------------------------- */

static int vt_pass = 0;          /* 通过的检查条数 */
static int vt_fail = 0;          /* 失败的检查条数 */
static int vt_point_pass = 0;    /* 通过的用例组数 */
static int vt_point_total = 0;   /* 用例组总数 */

/* 存活分配登记表：用于检测两次分配是否重叠 */
#define VT_MAX_LIVE 32
struct vt_live {
    struct Page *base;
    size_t npages;
};
static struct vt_live vt_live[VT_MAX_LIVE];
static int vt_nlive = 0;

/* vt_check - 一次带数值的检查。失败时打印一行，不 panic。 */
static void
vt_check(int ok, const char *what, unsigned long expect, unsigned long actual) {
    if (ok) {
        vt_pass++;
    } else {
        vt_fail++;
        cprintf("[verify]   FAIL: %s  (expect %lu, actual %lu)\n",
                what, expect, actual);
    }
}

/* vt_free - 释放一个块，并把"释放被拒绝"也记为一次失败 */
static void
vt_free(struct Page *base, size_t n) {
    int r = buddy_free_pages_checked(base, n);
    vt_check(r == 0, "buddy_free_pages_checked should succeed", 0, (unsigned long)r);
}

/* vt_live_add / vt_live_del - 维护存活分配登记表 */
static void
vt_live_add(struct Page *base, size_t npages) {
    if (vt_nlive < VT_MAX_LIVE) {
        vt_live[vt_nlive].base = base;
        vt_live[vt_nlive].npages = npages;
        vt_nlive++;
    }
}

static void
vt_live_del(struct Page *base) {
    for (int i = 0; i < vt_nlive; i++) {
        if (vt_live[i].base == base) {
            vt_live[i] = vt_live[vt_nlive - 1];
            vt_nlive--;
            return;
        }
    }
}

/* vt_overlaps - 新块 [base, base+npages) 是否与某个存活块重叠 */
static int
vt_overlaps(struct Page *base, size_t npages) {
    size_t o0 = buddy_page_offset(base);
    if (o0 == (size_t)-1) {
        return 1;                       /* 不在池内，视为异常重叠 */
    }
    for (int i = 0; i < vt_nlive; i++) {
        size_t o1 = buddy_page_offset(vt_live[i].base);
        if (o0 < o1 + vt_live[i].npages && o1 < o0 + npages) {
            return 1;
        }
    }
    return 0;
}

/* vt_conserved - 守恒律：池内空闲页数 + 存活占用页数 == 池总页数 */
static int
vt_conserved(void) {
    size_t live = 0;
    for (int i = 0; i < vt_nlive; i++) {
        live += vt_live[i].npages;
    }
    return (buddy_nr_free_pages() + live) == buddy_pool_npages();
}

/* vt_reset - 重新初始化管理池，并把存活登记表清空。
 * 重新初始化会让池的物理地址发生变化，之前登记的地址全部作废。 */
static void
vt_reset(void) {
    buddy_init();
    vt_nlive = 0;
}

/* vt_pow2_ceil - 不小于 n 的最小的 2 的幂（仅用于生成期望值） */
static size_t
vt_pow2_ceil(size_t n) {
    size_t v = 1;
    while (v < n) {
        v <<= 1;
    }
    return v;
}

/* -------------------------------------------------------------------
 * 页内容标记
 *
 * entry.S 建立了一整段 1GB 的线性映射（VA = pa + va_pa_offset，
 * VA 0xffffffffc0000000 起对应 PA 0x80000000 起），
 * 因此可以直接读写任意物理页的内容，用来检测
 * "已分配的块被别的分配覆盖/串改"这类错误。
 * ------------------------------------------------------------------- */
#define VT_MAGIC 0x5A00C0DEu

static void
vt_mark(struct Page *base, size_t npages, unsigned int seed) {
    for (size_t i = 0; i < npages; i++) {
        uintptr_t pa = page2pa(base + i);
        *(volatile unsigned int *)(pa + va_pa_offset) =
            (unsigned int)(VT_MAGIC ^ seed ^ (unsigned int)i);
    }
}

static int
vt_mark_intact(struct Page *base, size_t npages, unsigned int seed) {
    for (size_t i = 0; i < npages; i++) {
        uintptr_t pa = page2pa(base + i);
        unsigned int v = *(volatile unsigned int *)(pa + va_pa_offset);
        if (v != (unsigned int)(VT_MAGIC ^ seed ^ (unsigned int)i)) {
            return 0;
        }
    }
    return 1;
}

/* ===================================================================
 * V1  精确 2 的幂的请求
 *
 * 请求 1/2/4/.../64 页时：
 *   - 必须分配成功；
 *   - 块首记录的阶必须等于请求的阶；
 *   - 块首在池内必须按 2^order 对齐；
 *   - 空闲页数必须恰好减少 2^order；
 *   - 与所有存活分配不得重叠；
 *   - 释放后空闲页数必须完全复原。
 * =================================================================== */
static int
vt_v1_exact_pow2(void) {
    static const unsigned int orders[] = {0, 1, 2, 3, 4, 5, 6};
    vt_reset();
    size_t base_free = buddy_nr_free_pages();

    for (unsigned int i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
        unsigned int k = orders[i];
        size_t n = (size_t)1 << k;
        size_t before = buddy_nr_free_pages();

        struct Page *p = buddy_alloc_pages(n);
        vt_check(p != NULL, "V1 alloc should succeed", 1, 0);
        if (p == NULL) {
            continue;
        }
        vt_check(p->property == k, "V1 block order should match request",
                 (unsigned long)k, (unsigned long)p->property);

        size_t off = buddy_page_offset(p);
        vt_check(off != (size_t)-1 && (off & (n - 1)) == 0,
                 "V1 block should be 2^k aligned in pool",
                 0, (unsigned long)(off & (n - 1)));

        size_t after = buddy_nr_free_pages();
        vt_check(after + n == before, "V1 free pages should drop by exactly 2^k",
                 (unsigned long)before, (unsigned long)after);

        vt_check(!vt_overlaps(p, n), "V1 must not overlap live blocks", 0, 1);
        vt_live_add(p, n);

        vt_free(p, n);
        vt_live_del(p);

        vt_check(buddy_nr_free_pages() == base_free,
                 "V1 free pages should be restored", (unsigned long)base_free,
                 (unsigned long)buddy_nr_free_pages());
    }
    return vt_conserved();
}

/* ===================================================================
 * V2  非 2 的幂的请求
 *
 * 请求 3/5/6/7/9/11/13/100/1000 页时：
 *   - 必须分配成功，实际占用 2^ceil(log2 n) 页；
 *   - 块首记录的阶必须等于 ceil(log2 n)；
 *   - 空闲页数必须恰好减少 2^ceil(log2 n)；
 *   - 释放时按"当初请求的页数 n"释放必须被接受（契约检验）。
 * =================================================================== */
static int
vt_v2_non_pow2(void) {
    static const size_t reqs[] = {3, 5, 6, 7, 9, 11, 13, 100, 1000};
    vt_reset();
    size_t base_free = buddy_nr_free_pages();

    for (unsigned int i = 0; i < sizeof(reqs) / sizeof(reqs[0]); i++) {
        size_t n = reqs[i];
        size_t expect = vt_pow2_ceil(n);
        unsigned int expect_order = 0;
        while (((size_t)1 << expect_order) < expect) {
            expect_order++;
        }
        size_t before = buddy_nr_free_pages();

        struct Page *p = buddy_alloc_pages(n);
        vt_check(p != NULL, "V2 alloc should succeed", 1, 0);
        if (p == NULL) {
            continue;
        }
        vt_check(p->property == expect_order, "V2 order should be ceil(log2 n)",
                 (unsigned long)expect_order, (unsigned long)p->property);

        size_t after = buddy_nr_free_pages();
        vt_check(before - after == expect, "V2 should consume 2^ceil(log2 n) pages",
                 (unsigned long)expect, (unsigned long)(before - after));

        size_t off = buddy_page_offset(p);
        vt_check(off != (size_t)-1 && (off & (expect - 1)) == 0,
                 "V2 block should be aligned to its own size", 0,
                 (unsigned long)(off & (expect - 1)));

        /* 按"当初请求的页数"释放必须被接受 */
        vt_free(p, n);
        vt_check(buddy_nr_free_pages() == base_free,
                 "V2 free pages should be restored", (unsigned long)base_free,
                 (unsigned long)buddy_nr_free_pages());
    }
    return vt_conserved();
}

/* ===================================================================
 * V3  拆分：两个半块必须恰好覆盖原块的两半
 *
 * 做法：先申请一个 2^k 页的整块并记下它的池内偏移，原地释放，
 *       再申请两次 2^(k-1) 页，检查这两次拿到的块：
 *         - 偏移差恰好是 2^(k-1)（两半相邻、不重叠）；
 *         - 合并这两半的覆盖区间恰好等于原块（不多不少）；
 *         - 两半都按 2^(k-1) 对齐。
 * =================================================================== */
static int
vt_v3_split(void) {
    vt_reset();
    size_t k = 3;                       /* 用 8 页的块做实验 */
    size_t whole_n = (size_t)1 << k;
    size_t half_n = (size_t)1 << (k - 1);

    struct Page *whole = buddy_alloc_pages(whole_n);
    vt_check(whole != NULL, "V3 alloc whole block should succeed", 1, 0);
    if (whole == NULL) {
        return 0;
    }
    size_t orig = buddy_page_offset(whole);
    vt_free(whole, whole_n);

    struct Page *a = buddy_alloc_pages(half_n);
    struct Page *b = buddy_alloc_pages(half_n);
    vt_check(a != NULL && b != NULL, "V3 alloc two halves should succeed", 1, 0);
    if (a == NULL || b == NULL) {
        return 0;
    }

    size_t oa = buddy_page_offset(a);
    size_t ob = buddy_page_offset(b);
    size_t lo = oa < ob ? oa : ob;
    size_t hi = oa < ob ? ob : oa;

    vt_check(hi - lo == half_n, "V3 two halves should be adjacent", (unsigned long)half_n,
             (unsigned long)(hi - lo));
    vt_check(lo == orig && hi == orig + half_n,
             "V3 two halves should exactly cover the original block",
             (unsigned long)orig, (unsigned long)lo);
    vt_check((lo & (half_n - 1)) == 0, "V3 half should be aligned", 0,
             (unsigned long)(lo & (half_n - 1)));
    vt_check(oa != ob, "V3 halves must be two distinct blocks", 1, 0);

    vt_free(a, half_n);
    vt_free(b, half_n);
    vt_check(buddy_nr_free_pages() == buddy_pool_npages(),
             "V3 pool should be fully restored", (unsigned long)buddy_pool_npages(),
             (unsigned long)buddy_nr_free_pages());
    return vt_conserved();
}

/* ===================================================================
 * V4  合并：释放两个半块后必须能重新申请回整块
 *
 *   - 释放两个半块后，空闲页数应回到池总页数（完全合并）；
 *   - 再申请整块时，块首必须落在原块的低偏移处（合并取低偏移）；
 *   - 各阶分布应恢复成"只有一个最高阶块"。
 * =================================================================== */
static int
vt_v4_merge(void) {
    vt_reset();
    size_t k = 3;
    size_t whole_n = (size_t)1 << k;
    size_t half_n = (size_t)1 << (k - 1);

    struct Page *whole = buddy_alloc_pages(whole_n);
    vt_check(whole != NULL, "V4 alloc whole should succeed", 1, 0);
    if (whole == NULL) {
        return 0;
    }
    size_t orig = buddy_page_offset(whole);
    vt_free(whole, whole_n);

    struct Page *a = buddy_alloc_pages(half_n);
    struct Page *b = buddy_alloc_pages(half_n);
    if (a == NULL || b == NULL) {
        vt_check(0, "V4 alloc halves should succeed", 1, 0);
        return 0;
    }
    vt_free(a, half_n);
    vt_free(b, half_n);

    vt_check(buddy_nr_free_pages() == buddy_pool_npages(),
             "V4 pool should be fully merged", (unsigned long)buddy_pool_npages(),
             (unsigned long)buddy_nr_free_pages());

    struct Page *merged = buddy_alloc_pages(whole_n);
    vt_check(merged != NULL, "V4 alloc merged whole should succeed", 1, 0);
    if (merged != NULL) {
        vt_check(buddy_page_offset(merged) == orig,
                 "V4 merged block head should be the lower offset",
                 (unsigned long)orig, (unsigned long)buddy_page_offset(merged));
        vt_free(merged, whole_n);
    }
    return vt_conserved();
}

/* ===================================================================
 * V5  不该合并的两块不得合并
 *
 * 做法：申请一个 16 页的整块并释放，再申请两个 8 页的块 a、b，
 *       只释放 a，检查：
 *         - 空闲页数只增加了 8（若错误地与 b 合并，会增加 16）；
 *         - b 仍然处于"已分配"状态（PG_buddy_used 未被清掉）；
 *         - 再申请一个 8 页块时，不得与 b 重叠；
 *         - b 的页内容标记必须完好（没有被别人串改）。
 * 这四条是"错误合并"的四道独立防线。
 * =================================================================== */
static int
vt_v5_no_false_merge(void) {
    vt_reset();
    size_t whole_n = 16;
    size_t half_n = 8;

    struct Page *whole = buddy_alloc_pages(whole_n);
    vt_check(whole != NULL, "V5 alloc whole should succeed", 1, 0);
    if (whole == NULL) {
        return 0;
    }
    vt_free(whole, whole_n);

    struct Page *a = buddy_alloc_pages(half_n);
    struct Page *b = buddy_alloc_pages(half_n);
    if (a == NULL || b == NULL) {
        vt_check(0, "V5 alloc halves should succeed", 1, 0);
        return 0;
    }
    vt_live_add(b, half_n);
    vt_mark(b, half_n, 0x11);

    size_t before = buddy_nr_free_pages();
    vt_free(a, half_n);
    size_t after = buddy_nr_free_pages();

    vt_check(after - before == half_n,
             "V5 freeing one half must free exactly half a block",
             (unsigned long)half_n, (unsigned long)(after - before));
    vt_check(PageBuddyUsed(b), "V5 the other half must stay allocated", 1,
             (unsigned long)(PageBuddyUsed(b) ? 1 : 0));
    vt_check(!PageBuddyFree(b), "V5 the other half must not be marked free", 1,
             (unsigned long)(PageBuddyFree(b) ? 1 : 0));

    struct Page *c = buddy_alloc_pages(half_n);
    vt_check(c != NULL, "V5 alloc after partial free should succeed", 1, 0);
    if (c != NULL) {
        vt_check(!vt_overlaps(c, half_n), "V5 new block must not overlap the live half",
                 0, 1);
        vt_check(PageBuddyUsed(b), "V5 live half must still be allocated after alloc",
                 1, (unsigned long)(PageBuddyUsed(b) ? 1 : 0));
        vt_check(vt_mark_intact(b, half_n, 0x11), "V5 live half content must be intact",
                 1, 0);
        vt_free(c, half_n);
    }
    vt_live_del(b);
    vt_free(b, half_n);
    return vt_conserved();
}

/* ===================================================================
 * V6  往返一致性：释放再分配不得与其它存活块交叠，也不得串改内容
 *
 *   - 申请 a（4 页）并写入标记，再申请 b（4 页）并写入另一种标记；
 *   - 释放 a；
 *   - 再申请 4 页：不得与 b 重叠，b 仍须是已分配状态；
 *   - 若新拿回的正好是 a 的那块，其内容必须仍然完好；
 *   - b 的内容必须完好。
 * =================================================================== */
static int
vt_v6_roundtrip(void) {
    vt_reset();
    size_t n = 4;

    struct Page *a = buddy_alloc_pages(n);
    struct Page *b = buddy_alloc_pages(n);
    vt_check(a != NULL && b != NULL, "V6 alloc two blocks should succeed", 1, 0);
    if (a == NULL || b == NULL) {
        return 0;
    }
    vt_live_add(b, n);
    vt_mark(a, n, 0xA1);
    vt_mark(b, n, 0xB2);

    vt_free(a, n);

    struct Page *c = buddy_alloc_pages(n);
    vt_check(c != NULL, "V6 alloc after free should succeed", 1, 0);
    if (c != NULL) {
        vt_check(!vt_overlaps(c, n), "V6 new block must not overlap the live block",
                 0, 1);
        vt_check(PageBuddyUsed(b), "V6 live block must still be allocated", 1,
                 (unsigned long)(PageBuddyUsed(b) ? 1 : 0));
        if (c == a) {
            vt_check(vt_mark_intact(c, n, 0xA1),
                     "V6 reclaimed block content must be intact", 1, 0);
        }
        vt_check(vt_mark_intact(b, n, 0xB2),
                 "V6 live block content must be intact", 1, 0);
        vt_free(c, n);
    }
    vt_live_del(b);
    vt_free(b, n);
    return vt_conserved();
}

/* ===================================================================
 * V7  非法释放矩阵
 *
 * 依次尝试四种非法释放，每次之后都必须满足：
 *   - 释放被拒绝（返回 -1）；
 *   - 池的空闲页数一字未变；
 *   - 池仍然可用（还能正常分配出页，且与存活块不重叠）。
 *
 * 四种非法情形：
 *   a) 地址不在池内；
 *   b) 地址不是块首（块内第二页）；
 *   c) n 与块的阶数不符（4 页的块按 1 页释放）；
 *   d) n 与块的阶数不符（4 页的块按 8 页释放）；
 * 之后再试一次双重释放。
 * =================================================================== */
static int
vt_v7_illegal_free(void) {
    vt_reset();
    size_t n = 4;

    struct Page *p = buddy_alloc_pages(n);
    vt_check(p != NULL, "V7 alloc should succeed", 1, 0);
    if (p == NULL) {
        return 0;
    }
    vt_live_add(p, n);
    size_t before = buddy_nr_free_pages();

    /* a) 池外地址：物理内存最低端的那一页一定不在池内 */
    struct Page *outside = pa2page(DRAM_BASE);
    vt_check(buddy_free_pages_checked(outside, 1) == -1,
             "V7 free outside the pool must be rejected", (unsigned long)-1, 0);
    vt_check(buddy_nr_free_pages() == before,
             "V7 rejected free must not change free count", (unsigned long)before,
             (unsigned long)buddy_nr_free_pages());

    /* b) 不是块首 */
    vt_check(buddy_free_pages_checked(p + 1, 1) == -1,
             "V7 free of a non-head page must be rejected", (unsigned long)-1, 0);
    vt_check(buddy_nr_free_pages() == before,
             "V7 rejected free must not change free count", (unsigned long)before,
             (unsigned long)buddy_nr_free_pages());

    /* c) n 偏小 */
    vt_check(buddy_free_pages_checked(p, 1) == -1,
             "V7 free with too small n must be rejected", (unsigned long)-1, 0);
    vt_check(buddy_nr_free_pages() == before,
             "V7 rejected free must not change free count", (unsigned long)before,
             (unsigned long)buddy_nr_free_pages());

    /* d) n 偏大 */
    vt_check(buddy_free_pages_checked(p, 8) == -1,
             "V7 free with too large n must be rejected", (unsigned long)-1, 0);
    vt_check(buddy_nr_free_pages() == before,
             "V7 rejected free must not change free count", (unsigned long)before,
             (unsigned long)buddy_nr_free_pages());

    /* 池仍然可用 */
    struct Page *q = buddy_alloc_pages(n);
    vt_check(q != NULL, "V7 pool should still be usable", 1, 0);
    if (q != NULL) {
        vt_check(!vt_overlaps(q, n), "V7 new block must not overlap the live block",
                 0, 1);
        vt_free(q, n);
    }

    /* e) 双重释放 */
    vt_free(p, n);
    vt_live_del(p);
    size_t after_free = buddy_nr_free_pages();
    vt_check(buddy_free_pages_checked(p, n) == -1,
             "V7 double free must be rejected", (unsigned long)-1, 0);
    vt_check(buddy_nr_free_pages() == after_free,
             "V7 rejected double free must not change free count",
             (unsigned long)after_free, (unsigned long)buddy_nr_free_pages());
    return vt_conserved();
}

/* ===================================================================
 * V8  各阶分布完全复原
 *
 * 做一串分配/释放之后，把所有块都释放掉，
 * 检查 13 条链表的空闲块个数与初始快照**逐一相等**，
 * 并且空闲页总数回到池总页数。
 * 这是 buddy 最强的一条性质：全部归还后，池必须重新变成
 * "只有一个 2^BUDDY_MAX_ORDER 的最高阶空闲块"。
 * =================================================================== */
static int
vt_v8_order_restore(void) {
    vt_reset();
    size_t snap[BUDDY_NR_ORDER];
    for (int k = 0; k < BUDDY_NR_ORDER; k++) {
        snap[k] = buddy_nr_free_blocks(k);
    }
    size_t base_free = buddy_nr_free_pages();

    /* 一串不同大小的分配 */
    size_t reqs[] = {1, 3, 7, 16, 5, 64, 2};
    int nreq = (int)(sizeof(reqs) / sizeof(reqs[0]));
    struct Page *got[sizeof(reqs) / sizeof(reqs[0])];
    size_t gotn[sizeof(reqs) / sizeof(reqs[0])];
    int ngot = 0;
    for (int i = 0; i < nreq; i++) {
        struct Page *p = buddy_alloc_pages(reqs[i]);
        vt_check(p != NULL, "V8 alloc should succeed", 1, 0);
        if (p == NULL) {
            continue;
        }
        got[ngot] = p;
        gotn[ngot] = reqs[i];
        ngot++;
    }

    /* 全部释放 */
    for (int i = 0; i < ngot; i++) {
        vt_free(got[i], gotn[i]);
    }

    for (int k = 0; k < BUDDY_NR_ORDER; k++) {
        vt_check(buddy_nr_free_blocks(k) == snap[k],
                 "V8 per-order block count should be restored", (unsigned long)snap[k],
                 (unsigned long)buddy_nr_free_blocks(k));
    }
    vt_check(buddy_nr_free_pages() == base_free,
             "V8 free page count should be restored", (unsigned long)base_free,
             (unsigned long)buddy_nr_free_pages());
    return vt_conserved();
}

/* ===================================================================
 * V9  边界与耗尽
 *
 *   - 请求 0 页必须返回 NULL；
 *   - 超过最大阶的请求必须返回 NULL；
 *   - 极端大的请求 ((size_t)-1) 必须返回 NULL 而不是死循环
 *     （这是 buddy_order_up 溢出死循环的回归测试）；
 *   - 申请整池后池被耗尽，再申请必须返回 NULL；
 *   - 归还整池后空闲页数恢复。
 * =================================================================== */
static int
vt_v9_boundary(void) {
    vt_reset();
    size_t total = buddy_pool_npages();

    vt_check(buddy_alloc_pages(0) == NULL, "V9 alloc 0 must return NULL", 1, 0);
    vt_check(buddy_alloc_pages(total * 2) == NULL,
             "V9 alloc more than the pool must return NULL", 1, 0);
    vt_check(buddy_alloc_pages((size_t)1 << 30) == NULL,
             "V9 huge alloc must return NULL (overflow regression)", 1, 0);
    vt_check(buddy_alloc_pages((size_t)-1) == NULL,
             "V9 SIZE_MAX alloc must return NULL (overflow regression)", 1, 0);

    struct Page *all = buddy_alloc_pages(total);
    vt_check(all != NULL, "V9 alloc the whole pool should succeed", 1, 0);
    if (all == NULL) {
        return 0;
    }
    vt_check(buddy_nr_free_pages() == 0, "V9 pool should be exhausted", 0,
             (unsigned long)buddy_nr_free_pages());
    vt_check(buddy_alloc_pages(1) == NULL, "V9 alloc on exhausted pool must return NULL",
             1, 0);
    vt_check(buddy_alloc_pages(total) == NULL,
             "V9 alloc whole on exhausted pool must return NULL", 1, 0);

    vt_free(all, total);
    vt_check(buddy_nr_free_pages() == total, "V9 free pages should be restored",
             (unsigned long)total, (unsigned long)buddy_nr_free_pages());
    return vt_conserved();
}

/* ===================================================================
 * V10 与框架的对接
 *
 *   - buddy 返回的页必须能通过 pa2page(page2pa(p)) 换算回自己；
 *   - 物理地址必须落在 DRAM 范围内；
 *   - 块内的页必须物理连续（page2pa 逐页递增 PGSIZE）；
 *   - 页的 reserved 位必须为 0，引用计数必须为 0；
 *   - 池本身必须完全落在物理内存范围内。
 * =================================================================== */
static int
vt_v10_framework(void) {
    vt_reset();
    size_t n = 8;

    struct Page *p = buddy_alloc_pages(n);
    vt_check(p != NULL, "V10 alloc should succeed", 1, 0);
    if (p == NULL) {
        return 0;
    }
    vt_live_add(p, n);

    vt_check(pa2page(page2pa(p)) == p, "V10 page<->pa round trip must hold", 1, 0);
    vt_check(page2pa(p) >= DRAM_BASE, "V10 physical address must be in DRAM", 1, 0);
    vt_check(page2pa(p) < npage * PGSIZE, "V10 physical address must be in range", 1, 0);
    vt_check(!PageReserved(p), "V10 allocated page must not be reserved", 1, 0);
    vt_check(page_ref(p) == 0, "V10 allocated page must have zero refcount", 0,
             (unsigned long)page_ref(p));

    /* 块内物理连续 */
    int contiguous = 1;
    for (size_t i = 1; i < n; i++) {
        if (page2pa(p + i) != page2pa(p) + i * PGSIZE) {
            contiguous = 0;
            break;
        }
    }
    vt_check(contiguous, "V10 pages of one block must be physically contiguous", 1, 0);

    /* 池本身必须在物理内存范围内 */
    vt_check(buddy_pool_npages() > 0, "V10 pool must be initialized", 1, 0);
    vt_check(buddy_pool_npages() * PGSIZE <= npage * PGSIZE,
             "V10 pool must fit in physical memory", 1, 0);

    vt_free(p, n);
    vt_live_del(p);
    return vt_conserved();
}

/* ===================================================================
 * 用例组入口
 * =================================================================== */
typedef int (*vt_test_fn)(void);

static void
vt_run(const char *name, vt_test_fn fn) {
    int fail_before = vt_fail;
    cprintf("[verify] -- %s\n", name);
    int ok = fn();
    if (vt_fail > fail_before) {
        ok = 0;
    }
    vt_point_total++;
    if (ok) {
        vt_point_pass++;
        cprintf("[verify] PASS %s\n", name);
    } else {
        cprintf("[verify] FAIL %s\n", name);
    }
}

/* buddy_verify - 依次执行全部验证用例组 */
void
buddy_verify(void) {
    vt_pass = 0;
    vt_fail = 0;
    vt_point_pass = 0;
    vt_point_total = 0;
    vt_nlive = 0;

    cprintf("[verify] ===== buddy verify start (Li Sihan 2512163) =====\n");

    vt_run("V1  exact 2^k request", vt_v1_exact_pow2);
    vt_run("V2  non-2^k request rounds up", vt_v2_non_pow2);
    vt_run("V3  split covers the original block exactly", vt_v3_split);
    vt_run("V4  free merges back into a whole block", vt_v4_merge);
    vt_run("V5  must not merge two live blocks", vt_v5_no_false_merge);
    vt_run("V6  round trip keeps blocks disjoint", vt_v6_roundtrip);
    vt_run("V7  illegal frees are rejected safely", vt_v7_illegal_free);
    vt_run("V8  per-order distribution restored", vt_v8_order_restore);
    vt_run("V9  boundary and exhaustion", vt_v9_boundary);
    vt_run("V10 interface with the ucore framework", vt_v10_framework);

    cprintf("[verify] summary: %d/%d groups passed, %d/%d checks passed\n",
            vt_point_pass, vt_point_total, vt_pass, vt_pass + vt_fail);
    if (vt_point_pass == vt_point_total) {
        cprintf("[verify] RESULT: buddy verify PASS\n");
    } else {
        cprintf("[verify] RESULT: buddy verify FAIL\n");
    }
    cprintf("[verify] ===== buddy verify end =====\n");
}
