# Lab2：全过程标准提示词与验证记录

## 使用说明

本文件汇总本组在 Lab2 实验中使用的全部提示词，按实验指导书 lab0.5 要求的**四段式框架**组织：

```
[PROMPT]         做什么、改哪里、怎么改
[RELY]           可以依赖什么——宏定义、数据结构、已有函数
[GUARANTEE]      必须实现或修改什么——函数签名清单
[SPECIFICATION] 每个函数或行为的详细规格
```

**本组使用的 AI 工具**：Codex CLI　**底层模型**：`gpt-6.1-sol`　**参与成员**：2512163-李思翰、2512436-郭睿彤、2512130-刘婕

**记录原则**：
1. 所有提示词均按四段式框架整理；部分提示词为事后整理的标准格式，其原始输入与迭代反馈另行记录在对应小节。
2. 本组统一采用"基于任务而非基于函数""边界清楚""要求可验证""保留真实语义"四条原则。
3. 涉及运行结果的提示词均要求"区分观察与推断""不伪造运行结果"，且所有引用的运行数据来自实际执行。

---

## 全组提示词索引

| 序号 | 提示词主题 | 主要使用成员 | 对应代码 |
|------|-----------|------------|---------|
| 提示词一 | 物理内存探测与 Page 描述符初始化（任务一） | 2512163-李思翰 | `kern/mm/pmm.c` |
| 提示词二 | Best-Fit 分配器实现（任务二） | 2512436-郭睿彤 | `kern/mm/best_fit_pmm.c` |
| 提示词三 | Buddy System 模块生成（任务三 · 第一份） | 2512130-刘婕 | `kern/mm/buddy_pmm.c` |
| 提示词四 | Buddy System 测试函数生成（任务三 · 第二份） | 2512130-刘婕 | `kern/mm/buddy_test.c` |
| 提示词五 | SLUB 模块生成（任务四 · 第一份） | 2512130-刘婕 | `kern/mm/slub.c` |
| 提示词六 | SLUB 测试函数生成（任务四 · 第二份） | 2512130-刘婕 | `kern/mm/slub_test.c` |
| 提示词七 | Buddy 模块缺陷复核与修订（任务三 · 交叉复核阶段） | 2512163-李思翰 | `kern/mm/buddy_pmm.c`、`buddy_pmm.h` |
| 提示词八 | Buddy 独立外部验证层（任务三 · 交叉复核阶段） | 2512163-李思翰 | `kern/mm/buddy_verify.c`、`buddy_verify.h` |

> 提示词三至六由 2512130-刘婕 使用，共 4 组，与实验指导书"分别设计两份 prompt"的要求一一对应。
> 提示词七与提示词八由 2512163-李思翰 在任务三的交叉复核阶段使用：前者针对实现模块做缺陷定位与修订，后者独立编写外部验证层。

---

## 提示词一：物理内存探测与 Page 描述符初始化

**使用成员**：2512163-李思翰

```text
[PROMPT]
任务：修改 kern/mm/pmm.c，实现 page_init 函数，完成物理内存探测与 Page 描述符数组初始化，并把可用空闲页交给当前物理内存管理器。
操作要求：读取实际工程代码与设备树解析结果；只在标记位置补全，保留 pmm_init 等既有逻辑；设备树信息无效时必须终止初始化而非继续。
输出要求：直接修改文件；实现后说明 va/pa/页号/描述符地址四个概念如何区分，以及内核镜像与描述符数组为何都不能进入空闲区。

[RELY]
以下均为工程真实定义：
memlayout.h：KERNBASE 0xFFFFFFFFC0200000；PHYSICAL_MEMORY_OFFSET 0xFFFFFFFF40000000；PHYSICAL_MEMORY_END 0x88000000；KERNEL_BEGIN_PADDR 0x80200000。
struct Page { int ref; uint64_t flags; unsigned int property; list_entry_t page_link; };
PG_reserved=0，PG_property=1；SetPageReserved/ClearPageReserved/PageReserved/SetPageProperty/ClearPageProperty/PageProperty。
pmm.h：extern struct Page *pages; extern size_t npage; extern const size_t nbase; extern uint64_t va_pa_offset;
page2ppn/page2pa/pa2page；PADDR(kva) 宏；alloc_pages/free_pages/nr_free_pages；alloc_page()/free_page()。
mmu.h：PGSIZE 4096，PGSHIFT 12，PPN(la)。
dtb.h：get_memory_base()、get_memory_size()。
defs.h：ROUNDUP/ROUNDDOWN。
链接脚本 tools/kernel.ld 提供符号 end（.bss 末尾）、edata、etext。

[GUARANTEE]
必须实现：
void page_init(void);
使用框架已有的 init_memmap(base, n) 注册空闲区。
允许新增 static 辅助函数与局部变量。

[SPECIFICATION]
Pre-Condition：pmm_manager 已初始化，设备树已被 dtb_init 解析。
Post-Condition：
- va_pa_offset 设为 PHYSICAL_MEMORY_OFFSET。
- npage 表示物理页号的上界（等于 PPN(mem_base+mem_size)），不是页数。
- pages 指向内核镜像末尾按页对齐处的描述符数组，数组长度覆盖 nbase 到 npage 之间的每一页。
- 描述符数组所占空间、以及内核镜像所占空间，全部标记为保留。
- 剩余的完整物理页通过 init_memmap 交给 pmm_manager。
Case 1：设备树给出的基址或长度为 0，调用 panic 终止。
Case 2：可用完整页数为 0 时，不得调用 init_memmap。
Requirements：地址上界向下页对齐，末端不足一页的部分不交给分配器；描述符数组起点按页对齐；必须先把全部描述符标记为保留，再调用 init_memmap（init_memmap 内部有 assert(PageReserved(p))）。
```

**迭代说明**：本模块初版把 `npage` 写成页数语义，导致框架自带的 `test_lab2.c` 第 3 项检查报 `npage` 与期望不符；改为页号上界后通过。另一处问题是描述符数组的保留标记顺序，必须先全部标记保留再调用 `init_memmap`，否则触发 `assert(PageReserved(p))`。

---

## 提示词二：Best-Fit 分配器实现

**使用成员**：2512436-郭睿彤

```text
[PROMPT]
任务：在 kern/mm/best_fit_pmm.c 的标记位置实现完整的 Best-Fit 物理内存管理模块，并在 kern/mm/pmm.c 的 init_pmm_manager 中把当前管理器切换为 best_fit_pmm_manager。
操作要求：先阅读 kern/mm/pmm.h 中 struct pmm_manager 的定义与 kern/mm/default_pmm.c 的完整实现；只补全标记位置；不得修改文件中的 basic_check 与 best_fit_check 两个测试函数；不得修改全局 pmm_manager 之外的其他框架代码。
输出要求：直接修改文件；实现完成后说明 best_fit 与 first_fit 在扫描策略上的唯一差异，以及你如何满足测试函数隐含的状态约定。

[RELY]
struct pmm_manager 成员：name、init、init_memmap(base,n)、alloc_pages(n)、free_pages(base,n)、nr_free_pages、check。
kern/mm/default_pmm.c：first-fit 的完整实现，含 free_area_t 空闲链表、property 记录空闲块页数、list_add/list_del/list_next 的使用方式，可作为链表管理与合并逻辑的参照。
框架语义：property 表示空闲块含有的页数；PG_property 标志表示该页是空闲块的块首；nr_free_pages 返回空闲页总数。
测试函数隐含约定（从 basic_check 与 best_fit_check 反推）：模块内部必须有名为 free_list 的 list_entry_t 与名为 nr_free 的 unsigned int 变量，且 basic_check 会直接读写这两个符号。

[GUARANTEE]
必须提供并注册：
extern const struct pmm_manager best_fit_pmm_manager;
其 .check 指向 best_fit_check。
在 kern/mm/pmm.c 的 init_pmm_manager 中把 pmm_manager 指向 &best_fit_pmm_manager。
不得修改 basic_check 与 best_fit_check。

[SPECIFICATION]
Pre-Condition：框架已提供 basic_check 与 best_fit_check；default_pmm.c 可作为参照。
Post-Condition：best_fit_pmm_manager 六个接口全部可用；分配时选取满足容量的最小空闲块；释放时与物理相邻的空闲块合并。
Case 1：分配量大于空闲页总数时返回 NULL。
Case 2：释放的块与前驱或后继物理相邻时合并，合并后空闲块页数累加。
Case 3：best_fit_check 中先释放 p0+1 起 2 页与 p0+4 起 1 页，随后 alloc_pages(1) 必须返回 p0+4，alloc_pages(2) 必须返回剩余的 2 页块。
Requirements：链表必须按地址有序；空闲块的 property 与 nr_free 必须始终守恒；空闲块头必须带 PG_property 标志。
```

**迭代说明**：初版未定义 `free_list` 与 `nr_free` 这两个符号，`basic_check` 编译报错；按 `default_pmm.c` 的方式补上同名同类型的静态变量后通过。另一处是扫描策略最初写成 first-fit，`best_fit_check` 中 `assert(p0 + 4 == p1)` 失败，改为遍历全部块取最小者后通过。

---

## 提示词三：Buddy System 模块生成（任务三 · 第一份）

**使用成员**：2512130-刘婕

```text
[PROMPT]
任务：在 riscv64-ucore 内核中新建一个 Buddy System 页级物理内存分配器，文件为 kern/mm/buddy_pmm.c 和 kern/mm/buddy_pmm.h，并在 kern/init/init.c 的 kern_init() 中、pmm_init() 之后调用它。
操作要求：先阅读 kern/mm/pmm.h、kern/mm/memlayout.h、kern/mm/default_pmm.c、libs/list.h 与 kern/mm/pmm.c，确认框架实际提供的接口后再写代码；只在 kern/init/init.c 的指定位置插入调用，不改动 pmm_init 的既有语句；不要修改 kern/mm/memlayout.h 中 struct Page 的定义，它被其他模块与评测代码共用；不要把 buddy 注册为 struct pmm_manager 实例。
输出要求：直接创建并修改文件。实现完成后说明三件事：buddy 地址为何必须在页号空间计算、判定某个候选 buddy 是否空闲为何要同时检查两个条件、为什么不能把 buddy 注册为全局 pmm_manager。

[RELY]
以下信息摘自当前工程真实代码，请以此为准，不要臆造宏名或接口。

// kern/mm/memlayout.h —— 不可修改
struct Page {
    int ref;                        // 引用计数
    uint64_t flags;                 // 状态标志位，第 0、1 位已被框架占用
    unsigned int property;          // 空闲块页数
    list_entry_t page_link;         // 空闲链表链接
};
#define PG_reserved 0
#define PG_property 1
#define SetPageReserved(page)   ((page)->flags |= (1UL << PG_reserved))
#define ClearPageReserved(page) ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)      (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)   ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page) ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)      (((page)->flags >> PG_property) & 1)
#define le2page(le, member)     to_struct((le), struct Page, member)
typedef struct { list_entry_t free_list; unsigned int nr_free; } free_area_t;

// kern/mm/pmm.h
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
static inline ppn_t page2ppn(struct Page *page) { return page - pages + nbase; }
static inline uintptr_t page2pa(struct Page *page) { return page2ppn(page) << PGSHIFT; }
static inline struct Page *pa2page(uintptr_t pa);
static inline int page_ref(struct Page *page);
static inline void set_page_ref(struct Page *page, int val);
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
#define alloc_page()   alloc_pages(1)
#define free_page(page) free_pages(page, 1)
#define PADDR(kva)     /* 内核虚拟地址转物理地址，越界会 panic */

// kern/mm/mmu.h
#define PGSIZE  4096
#define PGSHIFT 12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)

// libs/list.h
void list_init(list_entry_t *elm);
void list_add(list_entry_t *listelm, list_entry_t *elm);
void list_add_before(list_entry_t *listelm, list_entry_t *elm);
void list_add_after(list_entry_t *listelm, list_entry_t *elm);
void list_del(list_entry_t *listelm);
void list_del_init(list_entry_t *listelm);
bool list_empty(list_entry_t *list);
list_entry_t *list_next(list_entry_t *listelm);
list_entry_t *list_prev(list_entry_t *listelm);

// libs/defs.h
#define ROUNDUP(a, n)   /* 向上取整到 n 的倍数 */
#define ROUNDDOWN(a, n) /* 向下取整到 n 的倍数 */

// libs/stdio.h
int cprintf(const char *fmt, ...);
// kern/debug/assert.h
#define assert(x) do { if (!(x)) { panic("assertion failed: %s", #x); } } while (0)
#define panic(...) __panic(__FILE__, __LINE__, __VA__)

[GUARANTEE]
必须实现以下接口，签名完全一致：

// kern/mm/buddy_pmm.h
#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__
#include <pmm.h>
#define BUDDY_MAX_ORDER 12
#define BUDDY_NR_ORDER  (BUDDY_MAX_ORDER + 1)
void buddy_init(void);
struct Page *buddy_alloc_pages(size_t n);
void buddy_free_pages(struct Page *base, size_t n);
size_t buddy_nr_free_pages(void);
size_t buddy_page_offset(struct Page *p);
#endif

你可以根据需要添加：
- 状态标志位的宏定义与取值（须使用 flags 中未被框架占用的位）
- static 辅助函数（阶数计算、块拆分、块合并、空闲块查找）
- 模块内部的静态变量（各阶空闲链表、各阶空闲块计数、池首指针、池页数）

[SPECIFICATION]
## buddy_init
Pre-Condition：全局 pmm_manager 已完成初始化，物理内存空闲区已建立；本函数在 pmm_init 之后被调用。
Post-Condition：buddy 池已建立，池内所有页归 buddy 内部管理，不再通过全局 alloc_pages/free_pages 单独管理；池的空闲链表反映池内全部可用空间。
Case 1：正常初始化
- 通过全局 alloc_pages 申请一块连续物理页作为管理池。
- 池首的物理地址不需要按 2^BUDDY_MAX_ORDER 对齐：本实现的 buddy 关系建立在"池内页偏移"上而非物理页号上，块在池内偏移上的对齐由算法自身保证；池只要求是物理连续的若干页，而 alloc_pages() 交回的块天然满足这一点。
- 整池作为一个最高阶的空闲块登记。
- 输出池的物理起始地址、页数与最大阶数。
Case 2：内存不足
- 若底层分配器返回 NULL，调用 panic 终止，错误信息需指出是池申请失败。
Requirements：池内偏移的异或关系是全部 buddy 计算的基础，不得改用物理页号或物理地址参与异或。

## buddy_alloc_pages
Pre-Condition：buddy_init 已完成；n 大于 0。
Post-Condition：成功时返回一个块首 Page，其后连续 2^order 页归调用方所有，order 是不小于 n 的最小 2 的幂的指数；返回的块首不携带空闲块标志；池内空闲页总数减少 2^order 页；失败时返回 NULL 且不改变任何状态。
Case 1：n 已是 2 的幂且有对应阶的空闲块，直接取出。
Case 2：n 不是 2 的幂，向上取整到 2 的幂后分配。例如 n=3 实际分配 4 页，n=5 实际分配 8 页。
Case 3：n 为 0，返回 NULL。
Case 4：所需阶数超过 BUDDY_MAX_ORDER，返回 NULL。
Case 5：池内无足够空闲空间，返回 NULL，不得死循环。
Case 6：阶数计算必须对任意 size_t 输入都能在有限步内返回，不得因左移溢出而死循环。
Case 7：需要从更高阶拆分
- 逐级二分拆分，每一级把右半块挂入对应阶数的空闲链表，左半块继续向下拆分，直到达到请求阶。
- 被拆出的右半块必须挂入正确的阶，且带空闲块标志。
Requirements：返回的块首必须清除空闲块标志并标记为已分配；拆分时右半块的偏移为 2^order 页。

## buddy_free_pages
Pre-Condition：base 指向此前由 buddy_alloc_pages 返回的块首；n 为该块的页数。
Post-Condition：base 指向的页全部回到可分配状态；若存在同阶的相邻空闲块则合并，合并后阶数提升，并继续向上检查；池内空闲页总数相应增加。
Case 1：正常释放，无同阶 buddy 可合并，登记到对应阶的空闲链表。
Case 2：存在同阶且空闲的 buddy
- 将 buddy 从其空闲链表摘下并与当前块合并。
- 合并后的块首必须取两个块中页偏移较小者。取错会导致高阶块不再满足对齐要求，后续所有 buddy 计算失效。
- 被并入的那一半退化为块内普通页，必须清掉其空闲标志与阶数记录，否则同一地址被重复释放时无法被检出。
- 合并后阶数加一，继续尝试向上合并。
Case 3：buddy 存在但不空闲，不合并。
Case 4：当前块已达最高阶，不再尝试合并。
Requirements：
- buddy 的计算必须基于块首在池内的页偏移与 2^order 异或，不能基于物理地址。
- 判定某个候选 buddy 是否空闲，必须同时检查该页的空闲块标志与它记录的阶数。只比较阶数不足以区分空闲块首与空闲块内部的页，会导致链表被破坏。
- 池边界外的偏移必须被识别为无 buddy，不得越界访问。
- 释放的地址若不是 buddy 分配出去的块首，应当 panic 而非静默忽略。
- 释放时必须校验参数 n 与块的阶数是否一致：n 必须满足 order_up(n) == 该块记录的阶数。"当初请求的页数"与"实际块大小"两种写法都应被接受，明显对不上的取值必须被拒绝且不改动任何状态。

## buddy_nr_free_pages
Pre-Condition：无。
Post-Condition：返回池内当前空闲的物理页总数。

## buddy_page_offset
Pre-Condition：无。
Post-Condition：返回该页在管理池内的页偏移；不在池内时返回一个明确的无效值。
```

**迭代说明**：本模块生成后经三次修改才通过编译与自检。

1. **首版把阶数写进 `property` 字段**，与框架中 `property` 表示"空闲块页数"的语义冲突，且 `best_fit_check` 会遍历空闲链表累加 `p->property` 并断言等于 `nr_free_pages()`，一旦 buddy 管理的页混入其中即破坏不变量。改为在 `flags` 的第 2、3 位定义 buddy 专用的空闲/已分配标志，阶数另存，`property` 保持框架语义不变。
2. **合并时块首取错**。首版合并后沿用原块首，导致新块的偏移不是 2^order 的倍数，后续按偏移异或算出的 buddy 全部指向错误位置。改为合并后取偏移较小者。
3. **判定 buddy 空闲只比较了阶数**。空闲块内部的页也残留着旧的 `property` 值，会被误判为块首并被 `list_del`，链表结构随即损坏。改为同时检查专用标志位与阶数。

> 此后任务三进入交叉复核阶段，由 2512163-李思翰 定位并修复了另外七处问题（FIX-1…FIX-7），其中 FIX-2 删除了初版冗余的池首物理对齐步骤、FIX-3 新增了带返回值的释放接口 `buddy_free_pages_checked()`、FIX-5 新增了两个只读观察接口 `buddy_pool_npages()` 与 `buddy_nr_free_blocks(int order)`。完整清单见报告第四章功能模块三的"交叉复核阶段发现并修复的七处问题"，对应的复核提示词见本文件提示词七。

---

## 提示词四：Buddy System 测试函数生成（任务三 · 第二份）

**使用成员**：2512130-刘婕

```text
[PROMPT]
任务：为 kern/mm/buddy_pmm.c 中已实现的 Buddy System 分配器编写一套自检测试，文件为 kern/mm/buddy_test.c 和 kern/mm/buddy_test.h，并在 kern/init/init.c 的 kern_init() 中、pmm_init() 之后调用。
操作要求：先通读 kern/mm/buddy_pmm.c 的实现，理解它维护了哪些不变量，再据此设计测试；测试只能通过 buddy_pmm.h 暴露的公开接口进行，不得访问模块内部的静态变量；不得修改 kern/mm/buddy_pmm.c 的实现代码；每个子测试结束后必须把管理池恢复到调用前的状态，使后续子测试的初始条件可预期。
输出要求：直接创建并修改文件。完成后说明每个子测试分别能证伪实现中的哪一类缺陷，以及为什么这些测试足以检验生成的实现。

[RELY]
buddy_pmm.h 暴露的公开接口：
void buddy_init(void);
struct Page *buddy_alloc_pages(size_t n);
void buddy_free_pages(struct Page *base, size_t n);
size_t buddy_nr_free_pages(void);
size_t buddy_page_offset(struct Page *p);
#define BUDDY_MAX_ORDER 12
#define BUDDY_NR_ORDER  (BUDDY_MAX_ORDER + 1)

实现中的关键不变量（设计测试的依据）：
- 管理池恰好是 2^BUDDY_MAX_ORDER 页。池处于完全合并的静止状态时，空闲页总数等于 2^BUDDY_MAX_ORDER，且池内只有一个最高阶空闲块。
- 每次分配使空闲页总数减少 2^order，order 是不小于请求量的最小 2 的幂的指数。
- 释放一个块后，池应能沿 buddy 合并链一路上行，重新回到完全合并的静止状态。
- 分配出的块首偏移必须按 2^order 对齐。

[GUARANTEE]
必须实现：
// kern/mm/buddy_test.h
#ifndef __KERN_MM_BUDDY_TEST_H__
#define __KERN_MM_BUDDY_TEST_H__
#include <pmm.h>
void buddy_test(void);
#endif

// kern/mm/buddy_test.c
void buddy_test(void);   // 依次调用各个 static 子测试
子测试函数必须是 static，且只在本文件内定义，不要在头文件中声明 static 函数。

[SPECIFICATION]
## buddy_test
Pre-Condition：buddy_init 已完成，池处于刚初始化的静止状态。
Post-Condition：全部子测试通过时输出一行 "buddy test: PASS"；任一断言失败由 assert 触发 panic。
Requirements：子测试按顺序执行，每个子测试以「池回到静止状态」作为结束条件。

## 子测试 1：基本分配与对齐性
- 分配 1 页，检查返回非 NULL、物理地址页对齐、空闲页总数恰好减少 1。
- 释放后检查空闲页总数回到 2^BUDDY_MAX_ORDER。

## 子测试 2：非 2 的幂请求的向上取整
- 分配 3 页，检查空闲页总数减少 4。
- 分配 5 页，检查空闲页总数减少 8。
- 分配 6 页，检查空闲页总数减少 8。
- 每次释放后检查池回到静止状态。

## 子测试 3：拆分行为
- 连续分配两个 1 页的对象，检查两者不同，且空闲页总数减少 2。
- 检查这两个块的页偏移之差为 1：分配器从池首逐级二分，切下的右半块正是所分配块的 order-0 buddy。
- 再分配 4 页，检查其页偏移按 4 对齐，空闲页总数累计减少 6。
- 全部释放后检查池回到静止状态。

## 子测试 4：buddy 合并
- 分配 1 页，此时被切下的右半块留在空闲链表中，正是它的 order-0 buddy。
- 释放该块，检查池能沿合并链一路上行，空闲页总数回到 2^BUDDY_MAX_ORDER。
- 再分配 2^BUDDY_MAX_ORDER 页，检查其页偏移为 0：合并后的块首必须是偏移较小者，否则高阶块不再对齐。
- 释放后检查池回到静止状态。

## 子测试 5：释放后重新分配可获得相同地址
- 分配 4 页，记录其物理地址与页偏移，释放。
- 再次分配 4 页，检查拿回同一物理地址与同一页偏移。
- 释放后检查池回到静止状态。

## 子测试 6：池边界不越界
- 分配并释放一个跨越池首的低位块，其 buddy 落在池外，合并逻辑必须识别并停止。
- 分配一个最大阶的块，释放时其 buddy 一定在池外。
- 释放后检查池回到静止状态。

## 子测试 7：耗尽时的失败行为
- 分配 2^BUDDY_MAX_ORDER 页，检查空闲页总数为 0。
- 再次分配 1 页、分配 2^BUDDY_MAX_ORDER 页、分配超出池大小的量、分配 0，均必须返回 NULL，不得崩溃或死循环。
- 归还后检查池回到静止状态。
```

**迭代说明**：首版子测试 1 断言"order-0 块的页偏移必须是偶数"，这与 buddy 的实际行为不符——buddy 恰恰会把奇数偏移的相邻块切下来，正确的性质是"同阶块偏移之差为 1"。断言写反导致测试必然失败，已按实际语义改写。另一处是子测试 4 原先只检查"空闲页总数守恒"，无法区分"合并成功"与"恰好数值相同"，改为增加"分配整池后块首偏移必须为 0"这一断言，直接锁定合并后块首的选择规则。

---

## 提示词五：SLUB 模块生成（任务四 · 第一份）

**使用成员**：2512130-刘婕

```text
[PROMPT]
任务：在 riscv64-ucore 内核中新建一个 SLUB 任意大小对象分配器，文件为 kern/mm/slub.c 和 kern/mm/slub.h，并在 kern/init/init.c 的 kern_init() 中调用它。
操作要求：先阅读 kern/mm/buddy_pmm.h 了解可用的页级分配接口，以及 kern/mm/memlayout.h、kern/mm/pmm.h、kern/mm/pmm.c；新建文件不要修改 struct Page 定义，也不要修改 buddy_pmm.c 的实现；只在 kern/init/init.c 的指定位置插入调用，且必须排在 buddy 相关的调用之后——SLUB 建立在 Buddy 之上，Buddy 的管理池尚未建立时申请页会失败。
输出要求：直接创建并修改文件。实现完成后说明两层架构中每层各负责什么、空闲对象如何借用自身空间存放 freelist 指针、尺寸类为何取 2 的幂、以及为什么 slab 描述符要用固定大小的静态数组而不是动态分配。

[RELY]
kern/mm/buddy_pmm.h 提供的页级接口（本模块的第一层）：
struct Page *buddy_alloc_pages(size_t n);
void buddy_free_pages(struct Page *base, size_t n);
size_t buddy_nr_free_pages(void);

kern/mm/pmm.h：
extern uint64_t va_pa_offset;
static inline uintptr_t page2pa(struct Page *page);
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);

kern/mm/mmu.h：PGSIZE 4096，PGSHIFT 12。
libs/list.h：list_init、list_add、list_del、list_next、list_empty。
libs/stdio.h：int cprintf(const char *fmt, ...);
kern/debug/assert.h：assert(x)、panic(...)。
libs/string.h：memset、memcpy。

[GUARANTEE]
必须实现以下接口，签名完全一致：

// kern/mm/slub.h
#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__
#include <pmm.h>
#define SLUB_NR_CLASS     9
#define SLUB_MAX_SLABS    64
#define SLUB_MAX_OBJECT   4096
extern const size_t slub_size_classes[SLUB_NR_CLASS];
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *obj);
size_t slub_nr_free_objects(void);
void slub_self_test(void);
#endif

你可以根据需要添加：
- slab 描述符结构体与静态数组
- static 辅助函数（尺寸类查找、页内对象切分、地址反查所属 slab）

[SPECIFICATION]
## slub_init
Pre-Condition：buddy 已完成初始化。
Post-Condition：所有 slab 描述符清空，各尺寸类的 slab 列表为空，模块处于可分配状态。

## slub_alloc
Pre-Condition：slub_init 已完成。
Post-Condition：成功时返回一个至少 size 字节的对象指针，其地址按该对象所属尺寸类的尺寸对齐，且该对象已从所属 slab 的空闲链表中摘除；失败时返回 NULL。
Case 1：size 为 0，返回 NULL。
Case 2：size 超过最大尺寸类，返回 NULL，不得静默分配更大的对象。
Case 3：尺寸类中已有空闲对象，从对应 slab 的空闲链表取出一个。
Case 4：尺寸类中没有空闲对象，向 buddy 申请一页，切分成若干等尺寸对象并串成空闲链表，取出其中一个返回。
Case 5：页级分配失败或 slab 描述符已用尽，返回 NULL。
Requirements：尺寸类集合固定为 2 的幂，最小档位不得小于 8 字节；空闲对象的前若干字节被借用来存放 freelist 指针，因此最小尺寸类必须能容纳一个指针。

## slub_free
Pre-Condition：obj 指向此前由 slub_alloc 返回的对象。
Post-Condition：obj 重新回到所属 slab 的空闲链表中，该 slab 的空闲计数加一。
Case 1：obj 为 NULL，无副作用直接返回。
Case 2：根据对象地址反查出所属 slab 与尺寸类，将其挂回该 slab 的空闲链表头。
Case 3：对象地址不属于任何 slab，调用 panic 终止，不得静默忽略。
Requirements：反查必须依据 slab 覆盖的地址区间与对象尺寸的对齐关系，不得只比较地址大小。

## slub_nr_free_objects
Pre-Condition：无。
Post-Condition：返回当前所有 slab 中尚未分配的对象总数。

## slub_self_test
Pre-Condition：slub_init 已完成。
Post-Condition：全部子测试通过时输出一行 "slub test: PASS"；任一断言失败由 assert 触发 panic。
Requirements：会耗尽页级资源的子测试必须放在最后，且结束后归还已分配对象，使后续使用不受影响。
```

**迭代说明**：首版在尺寸类数组定义处误用了 `SLUB_NR_ORDER` 宏（该宏属于 buddy 模块），编译报错，改为 `SLUB_NR_CLASS` 后通过。另一处是 `slub_free` 的反查逻辑只比较地址大小，会把落在同一页内但属于不同尺寸类的对象误判，改为按 slab 覆盖区间加对象对齐双重判定。

---

## 提示词六：SLUB 测试函数生成（任务四 · 第二份）

**使用成员**：2512130-刘婕

```text
[PROMPT]
任务：为 kern/mm/slub.c 中已实现的 SLUB 分配器编写一套自检测试，文件为 kern/mm/slub_test.c，并在 kern/init/init.c 的 kern_init() 中调用。
操作要求：先通读 kern/mm/slub.c 的实现，理解它的尺寸类划分、slab 切分方式与空闲链表组织，再据此设计测试；测试只能通过 slub.h 暴露的公开接口进行，不得访问模块内部的静态变量；不得修改 kern/mm/slub.c 的实现代码。
输出要求：直接创建并修改文件。完成后说明每个子测试分别能证伪实现中的哪一类缺陷，特别是如何证明不同对象之间不发生内存重叠。

[RELY]
slub.h 暴露的公开接口：
#define SLUB_NR_CLASS     9
#define SLUB_MAX_SLABS    64
#define SLUB_MAX_OBJECT   4096
extern const size_t slub_size_classes[SLUB_NR_CLASS];
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *obj);
size_t slub_nr_free_objects(void);
void slub_self_test(void);

实现中的关键性质（设计测试的依据）：
- 尺寸类集合为 2 的幂：16/32/64/128/256/512/1024/2048/4096。
- 请求量向上取整到所属尺寸类，返回对象地址按该尺寸对齐。
- 一个 slab 来自一页，页内被等分为若干同尺寸对象。
- 空闲对象的前 8 字节存放 freelist 指针。
- 新分配页的内存内容是未定义的，不能假设其初值。

[GUARANTEE]
必须实现：
// kern/mm/slub_test.c
void slub_self_test(void);
子测试函数必须是 static，只在本文件内定义。

[SPECIFICATION]
## slub_self_test
Pre-Condition：slub_init 已完成。
Post-Condition：全部子测试通过时输出一行 "slub test: PASS"；任一断言失败由 assert 触发 panic。
Requirements：子测试按顺序执行；会耗尽页级资源的子测试放在最后，且结束后归还全部已分配对象。

## 子测试 1：尺寸取整与对齐
- 分配 1 字节的对象，检查返回非 NULL 且地址按 16 字节对齐。
- 分配 17 字节的对象，检查地址按 32 字节对齐。
- 检查两次分配返回的地址不同。
- 释放两者。

## 子测试 2：对象隔离
- 分配两个 64 字节对象，检查地址不同。
- 先给第二个对象写入确定的字节模式，再写满第一个对象。
- 检查第二个对象的每个字节都保持原模式：若对象边界算错，写入第一个会破坏第二个。
- 同时检查第一个对象确实被写成了新模式，证明上述检查有效。
- 另用 256 字节对象重复一遍。
- 释放全部对象。
注意：不能直接断言"第二个对象的内容不是某个值"，因为新分配页的初值未定义，可能恰好等于该值。

## 子测试 3：释放后可复用
- 分配 128 字节对象，记录地址，释放。
- 再次分配 128 字节对象，检查拿回同一地址。
- 释放。

## 子测试 4：非法尺寸
- 分配 0 字节、分配超过最大尺寸类的量、分配极大值，均须返回 NULL。
- 释放 NULL 必须是无害的。

## 子测试 5：边界尺寸
- 对每一个尺寸类，分配恰好等于该尺寸的量，检查返回地址按该尺寸对齐。
- 逐个释放。

## 子测试 6：耗尽
- 循环申请最大尺寸的对象直到返回 NULL。
- 检查至少分配出过一些对象，且分配次数不超过 slab 描述符上限。
- 归还全部已分配对象。
```

**迭代说明**：首版子测试 2 写成"分配两个对象后，用 0xA5 填满第一个，再断言第二个对象的内容不等于 0xA5"。这个断言存在假失败风险——新分配页的初值未定义，第二个对象的内容可能恰好含有 0xA5。改为先给第二个对象写入确定模式、再写第一个、再校验第二个未被改动，同时增加"第一个对象确实被改成新模式"的自检，使断言既确定又能真正证伪重叠缺陷。

---

## 提示词七：Buddy 模块缺陷复核与修订（任务三 · 交叉复核阶段）

**使用成员**：2512163-李思翰

```text
[PROMPT]
任务：复核 kern/mm/buddy_pmm.c 与 kern/mm/buddy_pmm.h 这份 Buddy System 实现，逐条找出其中的正确性缺陷、健壮性缺口与可观测性缺失，给出可执行的修改清单。不修改 buddy_test.c 与 buddy_test.h（那是另一份独立产出）。
操作要求：先通读实现，再读 kern/mm/pmm.h、kern/mm/memlayout.h、kern/mm/default_pmm.c、kern/mm/best_fit_pmm.c 与 libs/list.h，确认框架对 property 字段、PG_property 标志、free_pages 返回值的确切约定；每条结论必须给出具体行号与"触发路径 → 后果"的推导，不接受"建议改进"这类无法验证的说法；对每条问题都给出具体改法。
输出要求：输出一份编号的修改清单，每条含问题描述、触发路径、后果严重度、具体改法。同时列出评审条目与修改清单条目数量若不一致，说明哪一条是故意不改的及理由。

[RELY]
框架约定（摘自真实代码）：
- property 字段：框架语义是"空闲块含多少页"，块首由 PG_property 标志位标识；框架的 free_pages 内部会 assert(!PageReserved(p) && !PageProperty(p))。
- 框架的 free_pages 返回 void，没有失败通道——"拒绝非法释放"无法通过返回值告知调用方。
- size_t 为 64 位无符号（libs/defs.h），左移溢出后 v 变为 0。
- 本实现的 buddy 关系建立在"池内页偏移"上（buddy_offset 用 (uintptr_t) 差值除以 sizeof(struct Page)），不是物理页号异或。
- 同一份实现另有一个姊妹文件 buddy_verify.c 使用外部验证层，不要修改它。

[GUARANTEE]
必须覆盖以下方面，逐条给出结论（确认无问题也要明确写"无问题"）：
- 阶数计算函数对极大 n 的行为，是否存在左移溢出导致的死循环；上层对阶数上限的检查是否真正可达。
- 初始化时申请的连续空间量是否必要；对齐步骤在"池内偏移异或"的关系定义下是否必需。
- 释放函数是否校验了 n 与块实际大小的一致性；不一致时会发生什么，是否静默。
- 头文件中的每个声明是否都有对应定义。
- 只读查询接口是否充分：能否从外部验证"释放后各阶分布完全复原"。
- 池内外判定与偏移计算所用运算是否在 C 标准下有定义。
- 空闲链表与各阶计数的生命周期是否一致：管理池释放后计数是否被清零。

[SPECIFICATION]
输出格式要求：
1. 先给一段不超过五条的总体结论，说明算法主干是否正确、需要改动的是局部还是设计骨架。
2. 再给一张表：实现是怎么工作的（每个函数一行行为说明 + 关键行号）。
3. 然后按严重度分三档（会导致错误结果 / 接口契约问题 / 健壮性与风格）逐条列出，每条含：问题代码片段、触发路径推导、后果、具体改法。
4. 对每条"故意不改"的条目，必须写明为什么它在本实现下成立、以及由哪条更严格的判据覆盖。
5. 最后给修改优先级表。
Requirements：
- 每条结论必须能从具体代码行推出，不得凭印象。
- 撤回上一版评审中不成立的批评，并逐条列出更正内容，避免后续按错误结论修改。
```

**迭代说明**：首版评审对若干条提出批评后，在复核过程中发现其中一部分并不成立，已逐条撤回并更正。典型的例子是"池首物理不对齐"这一条——它只在"物理页号异或"的 buddy 实现下才是问题，而本实现用池内偏移异或，因此该处的对齐步骤属于冗余而非必需。这个更正过程本身是本组的一条重要经验：**两个人共享同一个错误理解时，代码"看起来合理"不能作为它存在的理由。**

---

## 提示词八：Buddy 独立外部验证层（任务三 · 交叉复核阶段）

**使用成员**：2512163-李思翰

```text
[PROMPT]
任务：为 kern/mm/buddy_pmm.c 的 Buddy 分配器编写一套独立的外部验证代码，文件为 kern/mm/buddy_verify.c 与 kern/mm/buddy_verify.h，作为 buddy_test.c 之外的第二层验证。不得修改 buddy_pmm.c、buddy_pmm.h 与 buddy_test.c。
操作要求：只使用 buddy_pmm.h 暴露的公开接口，从外部观察分配器行为，不访问模块内部的静态变量；验证层与实现自检的失败行为必须不同——检查失败只累加失败计数并打印一行，绝不 panic，以免一条失败导致后续所有用例都跑不到；每组用例开始前重建一个可预期的初始状态，用例结束时核对状态守恒。
输出要求：直接创建并修改文件。完成后说明每组用例各自针对哪一类性质，以及为什么这些性质是"定义性的"（任何正确实现都必须满足），而不是某一种实现的策略细节。

[RELY]
buddy_pmm.h 暴露的公开接口：
void buddy_init(void);
struct Page *buddy_alloc_pages(size_t n);
void buddy_free_pages(struct Page *base, size_t n);
int buddy_free_pages_checked(struct Page *base, size_t n);
size_t buddy_nr_free_pages(void);
size_t buddy_nr_free_blocks(int order);
size_t buddy_pool_npages(void);
size_t buddy_page_offset(struct Page *p);
#define BUDDY_MAX_ORDER 12
#define BUDDY_NR_ORDER  (BUDDY_MAX_ORDER + 1)

实现的契约要点：
- 块的阶数由块首的 property 记录，实际大小 = 1 << property。
- 释放时传入的 n 必须与块的阶数一致（buddy_free_pages_checked 会校验），"当初请求的页数"与"实际块大小"两种写法都被接受。
- buddy_free_pages_checked 返回 0 表示成功、-1 表示拒绝，且拒绝时池的状态不发生任何变化。
- 块首的空闲/已分配状态可由 buddy_pmm.h 的 PageBuddyFree / PageBuddyUsed 宏读取。
- entry.S 建立了一整段 1GB 的线性映射（VA = pa + va_pa_offset），因此可以直接读写任意物理页的内容，用于检测"已分配的块被别的分配覆盖"这类错误。
- pmm.h 提供 page2pa、pa2page、page_ref、npage、DRAM_BASE。

[GUARANTEE]
必须实现：
// kern/mm/buddy_verify.h
#ifndef __KERN_MM_BUDDY_VERIFY_H__
#define __KERN_MM_BUDDY_VERIFY_H__
void buddy_verify(void);
#endif

// kern/mm/buddy_verify.c
void buddy_verify(void);          // 依次执行全部用例组，末尾打印一行 summary
检查函数：void vt_check(int ok, const char *what, unsigned long expect, unsigned long actual);
- ok 为真时通过计数加一；为假时打印一行 FAIL（含描述、期望值、实际值）并把失败计数加一。
- 绝不调用 panic 或 assert。

[SPECIFICATION]
判据设计原则（这是本文件最重要的要求）：
- 只断言由 Buddy System 的定义决定、任何正确实现都必须满足的性质。
- 不要把实现细节写死（例如"必须先拆左半再拆右半"），否则换一个同样正确的实现就会误报失败。
- 实现细节类性质由另一份测试文件覆盖，本文件不重复。

用例组要求（每组一个 static 函数，返回 0 表示通过）：
- 覆盖精确 2 的幂请求：阶数正确、池内按 2^order 对齐、空闲页数恰好减少 2^order、与所有存活分配不重叠、释放后完全复原。
- 覆盖非 2 的幂请求：实际占用 2^ceil(log2 n) 页、按"当初请求的页数"释放必须被接受。
- 覆盖拆分：先申请一个 2^k 整块并记下池内偏移，原地释放，再申请两次 2^(k-1)；检查两半相邻、覆盖区间恰好等于原块、两半各自对齐。
- 覆盖合并：释放两个半块后空闲页数回到池总页数，再申请整块时块首落在原块的低偏移处。
- 覆盖"不该合并的两块不得合并"：只释放一半时，空闲数只增一半、另一半仍为已分配状态、新分配的块不与之重叠、存活块的页内容标记完好。
- 覆盖往返一致性：释放再分配不得与其它存活块交叠，也不得串改其内容。
- 覆盖非法释放矩阵：池外地址、非块首、n 偏小、n 偏大、双重释放；每次都必须被拒绝，且池的空闲页数一字未变，池仍然可用。
- 覆盖各阶分布复原：一串不同大小的分配全部释放后，各阶空闲块个数与初始快照逐一相等，空闲页总数回到池总页数。
- 覆盖边界与耗尽：0 页、超过池大小、极大值、极端值都必须返回 NULL 而不是死循环；申请整池后池被耗尽，再申请必须返回 NULL；归还后空闲页数恢复。
- 覆盖与框架的对接：页号与物理地址往返一致、物理地址落在 DRAM 范围内、块内各页物理连续、已分配页的 reserved 位为 0 且引用计数为 0、池本身落在物理内存范围内。

通用要求：
- 维护一张"存活分配登记表"，记录每次分配占用的页数，用于检测两次分配是否重叠。
- 每组用例结束时核对守恒律：池内空闲页数 + 存活占用页数 == 池总页数。
- 用例组之间不得有状态残留：每组开始时重建管理池并清空登记表。
- 输出末尾打印一行 summary，格式为 "[verify] summary: N/M groups passed, K/J checks passed"，并在全部通过时打印 "[verify] RESULT: buddy verify PASS"。
```

**迭代说明**：初版曾把"块首相邻"这类依赖链表顺序的断言写进判据。复核后确认该断言在本实现下不依赖 LIFO 顺序（order-0 链表在首次分配后恰好只有一个块），但它仍**超出了 buddy 的定义**——定义只要求块是 2^k 页且按 2^k 对齐。改为断言"两个半块的覆盖区间恰好等于原块"，这条判据在两种拆分策略下都成立，是定义性的。

---

## 全组汇总说明

本组共记录八个阶段的提示词。其中：

- **提示词一**由 2512163-李思翰 在物理内存初始化阶段使用。
- **提示词二**由 2512436-郭睿彤 在 Best-Fit 分配器实现阶段使用。
- **提示词三至六**由 2512130-刘婕 在 Buddy System 与 SLUB 两个任务中使用，每项任务各两份提示词，分别用于生成实现模块与生成测试函数，与实验指导书的要求一一对应。
- **提示词七与提示词八**由 2512163-李思翰 在任务三的交叉复核阶段使用，分别用于 Buddy 模块的缺陷修订与独立外部验证层的编写。

与 Lab1 的根本差别在于：Lab1 的基础代码由指导书提供，提示词的重心在理解与验证；Lab2 的任务三、任务四不提供任何代码框架与测试函数，提示词的重心在于**把分配算法的行为规格写清楚，并在此基础上设计能证伪实现的测试**。

本组八组提示词中有四处反复出现的共同要求，都不是形式主义：

1. **区分观察与推断**。所有涉及运行结果的提示词都要求不得伪造运行结果，报告中引用的每一个数值都应可回溯到实际执行。
2. **状态约定必须显式声明**。`best_fit_check` 隐含要求模块内部存在 `free_list` 与 `nr_free` 两个特定符号，`buddy` 则要求空闲标志与阶数同时校验——这些约束若不写进提示词，AI 生成的代码会因"合理但不合约定"而无法通过检查。
3. **测试必须能证伪，而不是只验证不崩溃**。例如 SLUB 的对象隔离测试若写成"断言内容不等于某常量"，在新分配页初值未定义的前提下会假失败，也无法真正证伪重叠缺陷；必须写成"先建立确定模式再验证未被改动"。
4. **失败通道必须显式设计**。框架的 `free_pages` 返回 `void`，"拒绝非法释放"无法通知调用方，于是测试只能靠"赌内核不挂"来间接验证。提示词八因此要求实现侧提供带返回值的 `buddy_free_pages_checked()`，把拒绝行为变成一条可被程序验证的性质。

此外，提示词七记录了一条与前六条不同类型的经验：**复核阶段的提示词不能只要求"找 bug"，还必须要求"撤回不成立的批评"**。首版评审提出的若干条批评在复核后被证明不成立，若不明确撤回，后续会有人按错误结论去改本来正确的代码。

---

## 关于提示词记录范围的说明

本文件汇总的是**本次实验实际使用过的提示词**，不要求为报告的每一个章节单独配一条提示词。实验报告中的实验目的、整体逻辑分析与总结收获等章节属于对实验过程的归纳与表述，其写作依托于前述各阶段提示词所积累的结论，不需要另立提示词条目。
