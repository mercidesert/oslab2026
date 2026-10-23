#ifndef __KERN_MM_BUDDY_PMM_H__
#define  __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/* Buddy System —— 页级物理内存分配器
 *
 * 本模块是一个**独立于全局 pmm_manager 的二级分配器**：
 * 它不注册为 struct pmm_manager 实例，初始化时从当前 pmm_manager
 * 申请一块连续物理页作为自己的管理池，此后只在池内做拆分与合并。
 * 原因是实验评测会断言 pmm_manager == &best_fit_pmm_manager，
 * 替换全局管理器会使评测失败。 */

//lab2 function3 2512130

/* ===================================================================
 * 修订记录 —— 李思翰（2512163），2026-10-23
 *
 * 本头文件基于刘婕（2512130）交付的 T3/buddy_pmm(1).h 修改。
 * 修订点见 kern/mm/buddy_pmm.c 顶部的完整清单，此处只列接口层面的变化：
 *
 *   [R-1] 删除 void buddy_self_test(void) 的声明
 *         —— 原实现中该函数只有声明没有定义，一旦有人调用就是链接错误。
 *            实际使用的测试入口是 buddy_test.h 中的 void buddy_test(void)。
 *
 *   [R-2] 新增 int buddy_free_pages_checked(struct Page *base, size_t n)
 *         —— 带返回值的释放接口。框架的 free_pages 返回 void，没有失败通道，
 *            "拒绝非法释放"无法通知调用方；本接口返回 0 表示成功、
 *            -1 表示拒绝，并且拒绝时不改动任何状态。
 *
 *   [R-3] 新增 size_t buddy_pool_npages(void)
 *         新增 size_t buddy_nr_free_blocks(int order)
 *         —— 只读查询接口。没有它们就无法在池外观察池的真实规模与
 *            各阶空闲块分布，"释放后分布是否完全复原"这类最强判据
 *            也就无从验证。
 *
 *   [R-4] 补充 property 字段的语义说明与 free 的 n 契约说明（见下）。
 * =================================================================== */

/* 管理池的最大阶。2^BUDDY_MAX_ORDER 页为一个最大块。
 * 物理内存共 128MB = 32768 页，取 12 意味着最大块 4096 页 = 16MB。 */
#define BUDDY_MAX_ORDER 12
#define BUDDY_NR_ORDER  (BUDDY_MAX_ORDER + 1)

/* 状态标志位。框架已占用 PG_reserved(0) 与 PG_property(1)，
 * buddy 使用第 2、3 位，与框架语义完全隔离。 */
#define PG_buddy_free   2   /* 该页是某个 buddy 空闲块的块首 */
#define PG_buddy_used   3   /* 该页是某个 buddy 已分配块的块首 */

#define SetPageBuddyFree(page)  ((page)->flags |= (1UL << PG_buddy_free))
#define ClearPageBuddyFree(page) ((page)->flags &= ~(1UL << PG_buddy_free))
#define PageBuddyFree(page)     (((page)->flags >> PG_buddy_free) & 1)
#define SetPageBuddyUsed(page)  ((page)->flags |= (1UL << PG_buddy_used))
#define ClearPageBuddyUsed(page) ((page)->flags &= ~(1UL << PG_buddy_used))
#define PageBuddyUsed(page)     (((page)->flags >> PG_buddy_used) & 1)

/* -------------------------------------------------------------------
 * [R-4] property 字段的语义
 *
 *   框架约定：空闲块块首的 property = 该空闲块的**页数**
 *             （见 kern/mm/default_pmm.c:26, 55, 65, 106）
 *   本模块约定：块首的 property = 该块的**阶(order)**
 *             实际块大小 = (size_t)1 << property
 *
 * 两套语义作用在不同的页集合上（本模块的池不交还给框架的空闲链表），
 * 因此互不干扰。判断一个块的大小时请用 1 << property，不要直接把
 * property 当页数用。
 * ------------------------------------------------------------------- */

/* 初始化：从当前 pmm_manager 申请一个 2^BUDDY_MAX_ORDER 页连续的管理池。
 * 池首的物理地址无需按 2^BUDDY_MAX_ORDER 对齐 —— 本模块的所有 buddy
 * 计算都基于"池内页偏移"，块在池内偏移上的对齐由算法自身保证。
 * 详见 buddy_pmm.c 中 [FIX-2] 的说明。 */
void buddy_init(void);

/* 分配 n 个连续物理页，n 不是 2 的幂时向上取整，返回块首。
 * n == 0 或超出最大阶时返回 NULL。 */
struct Page *buddy_alloc_pages(size_t n);

/* 释放以 base 为首的连续块。
 *
 * [R-2] n 必须满足 buddy_order_up(n) == base->property，
 *       传"当初请求的页数"或"实际块大小"均可；
 *       不满足时拒绝释放并报错，块保持已分配状态。
 *
 * 本函数在参数非法时 panic（与池外地址 / 非块首 / 非法阶数三处
 * 既有检查保持一致）；若调用方需要自行处理非法参数，请改用
 * buddy_free_pages_checked()。 */
void buddy_free_pages(struct Page *base, size_t n);

/* [R-2] 带返回值的释放接口。
 * 返回 0 = 释放成功；-1 = 拒绝（n 与块阶数不符 / 地址不在池内 /
 * 不是 buddy 分配的块首 / n == 0），且拒绝时不改动任何状态。 */
int buddy_free_pages_checked(struct Page *base, size_t n);

/* 池内当前空闲页总数。池未初始化或已释放时返回 0。 */
size_t buddy_nr_free_pages(void);

/* [R-3] 某一阶的空闲块个数。order 越界时返回 0。 */
size_t buddy_nr_free_blocks(int order);

/* [R-3] 管理池的总页数。池未初始化时返回 0。 */
size_t buddy_pool_npages(void);

/* 测试辅助：返回页在管理池内的页偏移，池外返回 (size_t)-1 */
size_t buddy_page_offset(struct Page *p);

#endif /* ! __KERN_MM_BUDDY_PMM_H__ */
