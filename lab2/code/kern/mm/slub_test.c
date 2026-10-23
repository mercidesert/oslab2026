#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>

//lab2 function4 2512130

/* 全部子测试通过时输出这一行 */
#define SLUB_TEST_PASS "slub test: PASS"

/* 子测试 1：尺寸取整与对齐
 *
 * 验证目标：请求被正确归入尺寸类，返回的对象按其尺寸类对齐。
 */
static void slub_test_size_class(void) {
    /* 请求 1 字节应落在 16 字节类 */
    void *p1 = slub_alloc(1);
    assert(p1 != NULL);
    assert(((uintptr_t)p1 & 15) == 0);

    /* 请求 17 字节应落在 32 字节类 */
    void *p2 = slub_alloc(17);
    assert(p2 != NULL);
    assert(((uintptr_t)p2 & 31) == 0);

    /* 同一次分配中的两个对象地址必须不同 */
    assert(p1 != p2);

    slub_free(p1);
    slub_free(p2);
}

/* 子测试 2：对象隔离
 *
 * 验证目标：相邻对象之间没有内存重叠。
 * 若切分时对象边界算错，写入一个对象会破坏另一个。
 *
 * 注意：新分配的页来自空闲物理内存，内容是未定义的，
 * 因此不能直接断言「b 的内容不是某个值」。正确做法是
 * 先给 b 写入确定的模式，再写 a，然后检查 b 未被改动。
 */
static void slub_test_isolation(void) {
    unsigned char *a = (unsigned char *)slub_alloc(64);
    unsigned char *b = (unsigned char *)slub_alloc(64);
    assert(a != NULL && b != NULL);
    assert(a != b);

    /* 先给 b 建立确定的模式 */
    memset(b, 0x5A, 64);
    /* 再写满 a 的 64 字节 */
    memset(a, 0xA5, 64);
    /* b 的 64 字节必须一个字节都没被改动 */
    for (int i = 0; i < 64; i++) {
        assert(b[i] == 0x5A);
    }
    /* a 自己确实被写成了 0xA5，证明上面的检查有效 */
    for (int i = 0; i < 64; i++) {
        assert(a[i] == 0xA5);
    }

    /* 更大尺寸的对象同样不能互相破坏 */
    unsigned char *c = (unsigned char *)slub_alloc(256);
    unsigned char *d = (unsigned char *)slub_alloc(256);
    assert(c != NULL && d != NULL);
    memset(d, 0x3C, 256);
    memset(c, 0xC3, 256);
    for (int i = 0; i < 256; i++) {
        assert(d[i] == 0x3C);
    }

    slub_free(a);
    slub_free(b);
    slub_free(c);
    slub_free(d);
}

/* 子测试 3：释放后可复用
 *
 * 验证目标：释放的对象回到 freelist，能被再次分配到。
 */
static void slub_test_reuse(void) {
    void *p = slub_alloc(128);
    assert(p != NULL);
    uintptr_t addr = (uintptr_t)p;

    slub_free(p);

    void *q = slub_alloc(128);
    assert(q != NULL);
    /* 刚释放的对象应当被优先复用 */
    assert((uintptr_t)q == addr);

    slub_free(q);
}

/* 子测试 4：非法尺寸
 *
 * 验证目标：size 为 0 或超过最大尺寸时返回 NULL。
 */
static void slub_test_invalid_size(void) {
    assert(slub_alloc(0) == NULL);
    assert(slub_alloc(SLUB_MAX_OBJECT + 1) == NULL);
    assert(slub_alloc((size_t)1 << 30) == NULL);
    /* 释放 NULL 应当是无害的 */
    slub_free(NULL);
}

/* 子测试 5：边界尺寸
 *
 * 验证目标：恰好等于某个尺寸类的请求被正确处理。
 */
static void slub_test_boundary_size(void) {
    for (int i = 0; i < SLUB_NR_CLASS; i++) {
        void *p = slub_alloc(slub_size_classes[i]);
        assert(p != NULL);
        /* 对象地址必须按其尺寸类对齐 */
        assert(((uintptr_t)p & (slub_size_classes[i] - 1)) == 0);
        slub_free(p);
    }
}

/* 子测试 6：耗尽
 *
 * 验证目标：buddy 池耗尽后返回 NULL 而不是崩溃或死循环。
 *
 * 本子测试会耗尽 buddy 池，因此在所有会改变池状态的子测试
 * 之后执行。结束时把已分配的对象全部释放，使 buddy 池恢复
 * 到完全空闲的状态，后续测试可以继续正常使用。
 */
static void slub_test_exhaust(void) {
    /* 记录分配前的对象总数，用于结束时的守恒核对 */
    size_t before = slub_nr_free_objects();

    /* 不断申请大对象直到失败。这里用一个静态数组保存已分配的
     * 指针，以便结束时归还。数组大小取 SLUB_MAX_SLABS，因为
     * 每分配一个 4096 字节对象至多消耗一个 slab。 */
    static void *held[SLUB_MAX_SLABS];
    size_t count = 0;

    void *p;
    while ((p = slub_alloc(SLUB_MAX_OBJECT)) != NULL) {
        assert(count < SLUB_MAX_SLABS);
        held[count++] = p;
    }

    /* 至少分配出过一些对象，说明路径本身是通的 */
    assert(count > 0);
    /* slab 描述符耗尽或 buddy 池耗尽时都必须返回 NULL 而非死循环 */
    assert(count <= SLUB_MAX_SLABS);

    /* 归还全部对象，使池恢复 */
    for (size_t i = 0; i < count; i++) {
        slub_free(held[i]);
    }

    /* 释放后空闲对象数应回到初始值（slab 本身仍被占用，
     * 因此这里只要求不小于初始值，且对象已全部回到 freelist） */
    assert(slub_nr_free_objects() >= before);
}

/* slub_self_test - 依次执行全部子测试 */
void slub_self_test(void) {
    slub_test_size_class();
    slub_test_isolation();
    slub_test_reuse();
    slub_test_invalid_size();
    slub_test_boundary_size();
    slub_test_exhaust();

    cprintf("%s\n", SLUB_TEST_PASS);
}
