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
| 提示词二之二 | SLUB 扩展测试设计 | 2512436-郭睿彤 | `kern/mm/slub_test.c` |
| 提示词三 | Buddy System 模块生成（任务三 · 第一份） | 2512130-刘婕 | `kern/mm/buddy_pmm.c` |
| 提示词四 | Buddy System 测试函数生成（任务三 · 第二份） | 2512130-刘婕 | `kern/mm/buddy_test.c` |
| 提示词五 | SLUB 模块生成（任务四 · 第一份） | 2512130-刘婕 | `kern/mm/slub.c` |
| 提示词六 | SLUB 测试函数生成（任务四 · 第二份） | 2512130-刘婕 | `kern/mm/slub_test.c` |
| 提示词七 | Buddy 模块缺陷复核与修订（任务三 · 交叉复核阶段） | 2512163-李思翰 | `kern/mm/buddy_pmm.c`、`buddy_pmm.h` |
| 提示词八 | Buddy 独立外部验证层（任务三 · 交叉复核阶段） | 2512163-李思翰 | `kern/mm/buddy_verify.c`、`buddy_verify.h` |
| 附录 | 李思翰组提示词原文（T1-1 / T1-2 / T1-3 / T3-2） | 2512163-李思翰 | `kern/mm/pmm.c`、`buddy_verify.c`、`buddy_test.c` |

> 提示词三至六由 2512130-刘婕 使用，共 4 组，与实验指导书"分别设计两份 prompt"的要求一一对应。
> 提示词七与提示词八由 2512163-李思翰 在任务三的交叉复核阶段使用：前者针对实现模块做缺陷定位与修订，后者独立编写外部验证层。
> 提示词二之二是 2512436-郭睿彤 在任务四阶段补充的 SLUB 扩展测试设计，与 `slub_test.c` 配合使用。
> 附录部分收录 2512163-李思翰 独立撰写的四组提示词**原文**（任务一的三段迭代与任务三的外部验证层），按分工表要求只汇总、不改写；其与正文各小节的对应关系见附录开头。

---

---

## 附录：2512163-李思翰组提示词原文（任务一 · 三段迭代；任务三 · 外部验证层）

本附录按分工表要求**只汇总、不改写原文**，收录 2512163-李思翰 独立设计并实际使用的四组提示词原文。三段 T1 提示词合成一组，对应 `kern/mm/pmm.c` 中 `page_init()` 的实现、重构与逐条自查；T3-2 是他在任务三交叉复核阶段独立设计并执行的外部验证层提示词，产出 `kern/mm/buddy_verify.c` / `buddy_verify.h`。

| Prompt | 实际产出 |
| --- | --- |
| T1-1 | `kern/mm/pmm.c` 中 `page_init()` 的初版实现 |
| T1-2 | 重构后的 `page_init()`（改用 `pa2page()` / `PADDR()` / `ROUNDUP()` 框架入口，即仓库现行版本），并补上 `mem_end < mem_base` 溢出检查 |
| T1-3 | 逐条自查结论，确认「没有可用完整页面时不调用 `init_memmap()`」「描述符数组不进空闲区」等硬性约束成立 |
| T3-2 | `kern/mm/buddy_verify.c` + `kern/mm/buddy_verify.h`（外部验证层，10 组用例 / 205 条计数式断言），以及 `kern/mm/buddy_test.c` 中补齐的 7 个子测试注释 |

### 原文分组与本文件既有小节的对应关系

| 原文编号 | 对应本文件小节 |
| --- | --- |
| T1-1 / T1-2 / T1-3 | 提示词一（物理内存探测与 Page 描述符初始化）的实现依据 |
| T3-2 | 提示词八（Buddy 独立外部验证层）的原文出处 |

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

> 以下为实际使用的提示词原文（由 2512436-郭睿彤提供）。它采用自然语言形式而非四段式模板，但**十条要求逐条对应四段式的四个要素**，末尾附有整理后的四段式版本以供对照。

### 实际使用的提示词原文

```text
我正在完成 ucore Lab2 的 Best-Fit 连续物理内存分配任务。

请阅读以下文件的接口和现有实现：
- kern/mm/pmm.h
- kern/mm/memlayout.h
- kern/mm/default_pmm.c
- libs/list.h
- kern/mm/best_fit_pmm.c

请只补全 kern/mm/best_fit_pmm.c 中 `//lab2 function2-2 YOUR CODE` 所需的内容，并在 kern/mm/pmm.c 的 init_pmm_manager() 中启用 best_fit_pmm_manager。

要求：
1. 不得修改 basic_check() 和 best_fit_check()。
2. 使用 free_area、free_list 和 nr_free 管理空闲连续物理页块。
3. 实现 init、init_memmap、alloc_pages、free_pages、nr_free_pages。
4. 分配 n 页时，遍历所有空闲块，选择长度大于等于 n 的最小空闲块；长度相同时选地址较低的块。
5. 分配后若有剩余页，正确拆分剩余空闲块。
6. 释放后按物理地址顺序插入空闲链表，并合并前后连续空闲块。
7. 正确维护 Page 的 flags、property、ref 和 nr_free。
8. 实现并导出 const struct pmm_manager best_fit_pmm_manager。
9. 不修改 page_init()，不实现 Buddy 或 SLUB。
10. 请输出可直接替换进文件的完整 best_fit_pmm.c，并说明 pmm.c 需要修改的那一行。
```

### 四段式整理版（对照用）

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

> 上面十条要求与四段式的对应关系：**第 1、9 条 → [GUARANTEE]**（不得修改的边界）；**第 2、3、7 条 → [GUARANTEE] 的接口清单与 [RELY]**；**第 4、5、6 条 → [SPECIFICATION] 的 Case**；**"请阅读以下文件"→ [RELY]**；**"请输出完整文件并说明要改哪一行"→ [PROMPT] 与 [GUARANTEE]**。

---

## 提示词二之二：SLUB 扩展测试设计

**使用成员**：2512436-郭睿彤

> 这份提示词由 2512436-郭睿彤 提供，用于在 `slub_test.c` 之外继续加强 SLUB 的验证强度。它延续了任务四测试设计的思路，把测试维度从"六个子测试"扩展到"每个 size class 的边界请求、跨 slab 分配、释放复用、耗尽恢复、随机压力"五个方面。

```text
请先阅读团队当前的 SLUB 源码、相关头文件和页分配器接口，确认实际存在的函数名、对象分配接口、对象释放接口、size class、对象对齐方式、每个 slab 的对象容量，以及空 slab 的回收或缓存策略。

在不修改 SLUB 正式实现逻辑的前提下，为该实现编写并接入测试代码。不要假设不存在的接口；如果接口或统计字段缺失，先说明需要绑定的位置。

测试必须覆盖：

1. 对每个实际 size class 测试边界请求：
   - s - 1；
   - s；
   - s + 1。
   验证请求会进入正确的 size class，且对象满足实现约定的地址对齐。

2. 跨 slab 分配：
   对每一个 size class，连续分配超过单个 slab 容量的对象，确认能够创建或使用多个 slab，且所有活跃对象地址不重叠。

3. 释放与复用：
   释放部分对象后再次申请，验证幸存对象数据未被改写；允许实现复用空闲对象，但不得出现两个活跃对象占用同一地址。

4. 耗尽与恢复：
   在受控的测试条件下持续分配，直到后端无法再提供页或对象分配返回失败。失败后释放测试持有的对象，再次分配应能恢复。测试结束必须释放自己申请的全部对象。

5. 随机压力测试：
   使用固定随机种子 2512436，执行至少 10000 次混合 alloc/free。为每个活跃对象写入唯一字节模式，在释放前和随机检查时验证内容未损坏。记录对象地址、请求大小和操作序号，用于定位失败。

6. 输出：
   每一组测试应输出 PASS 或 FAIL；输出测试前后的活跃对象数、测试持有对象数，以及实现实际提供的页/slab统计。若空 slab 被缓存，不应把暂未归还给页分配器的缓存页直接判定为泄漏。

不要对 0 字节请求、重复释放、NULL 释放或超过最大 class 的请求自行假设行为。先根据实际 SLUB 接口说明这些行为是否定义；对未定义行为不要在正式测试中调用。

请给出：
- 需新增或修改的测试文件；
- 编译接入位置；
- 完整测试代码；
- 运行命令；
- 每个测试检查的核心性质；
- 预期输出格式。
```

> **这份提示词里有三处值得记录的设计考虑**：
>
> **第一，"不要假设不存在的接口"是前置约束而非事后说明。** 提示词要求先"确认实际存在的函数名、统计字段"，并在缺失时"先说明需要绑定的位置"——把"接口可能不存在"这件事从测试失败的原因里剥离出来，变成实现阶段就要澄清的问题。
>
> **第二，"固定随机种子"让压力测试可复现。** 用学号 2512436 作种子，任何一次失败都能按同样的序列重放。
>
> **第三，对未定义行为的态度是"先查证，不假设"。** 提示词明确要求不自行假设 0 字节请求、重复释放、NULL 释放、超上限请求的行为，**并且不要在正式测试中调用它们**。这是把"非法输入测试"从断言模块契约改为澄清模块契约——因为这些行为在 SLUB 的实现里本就未被定义，测它们等于替实现者做决定。

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

本组共记录九个阶段的提示词。其中：

- **提示词一**由 2512163-李思翰 在物理内存初始化阶段使用。
- **提示词二**由 2512436-郭睿彤 在 Best-Fit 分配器实现阶段使用，附有该提示词的**原始输入原文**。
- **提示词三至六**由 2512130-刘婕 在 Buddy System 与 SLUB 两个任务中使用，每项任务各两份提示词，分别用于生成实现模块与生成测试函数，与实验指导书的要求一一对应。
- **提示词七与提示词八**由 2512163-李思翰 在任务三的交叉复核阶段使用，分别用于 Buddy 模块的缺陷修订与独立外部验证层的编写。
- **提示词二之二**由 2512436-郭睿彤 提供，是 SLUB 的扩展测试设计，与提示词六配合构成任务四的完整测试方案。

与 Lab1 的根本差别在于：Lab1 的基础代码由指导书提供，提示词的重心在理解与验证；Lab2 的任务三、任务四不提供任何代码框架与测试函数，提示词的重心在于**把分配算法的行为规格写清楚，并在此基础上设计能证伪实现的测试**。

本组提示词中有五处反复出现的共同要求，都不是形式主义：

1. **区分观察与推断**。所有涉及运行结果的提示词都要求不得伪造运行结果，报告中引用的每一个数值都应可回溯到实际执行。
2. **状态约定必须显式声明**。`best_fit_check` 隐含要求模块内部存在 `free_list` 与 `nr_free` 两个特定符号，`buddy` 则要求空闲标志与阶数同时校验——这些约束若不写进提示词，AI 生成的代码会因"合理但不合约定"而无法通过检查。
3. **测试必须能证伪，而不是只验证不崩溃**。例如 SLUB 的对象隔离测试若写成"断言内容不等于某常量"，在新分配页初值未定义的前提下会假失败，也无法真正证伪重叠缺陷；必须写成"先建立确定模式再验证未被改动"。
4. **失败通道必须显式设计**。框架的 `free_pages` 返回 `void`，"拒绝非法释放"无法通知调用方，于是测试只能靠"赌内核不挂"来间接验证。提示词八因此要求实现侧提供带返回值的 `buddy_free_pages_checked()`，把拒绝行为变成一条可被程序验证的性质。
5. **未定义行为不得由测试替实现者做决定**。SLUB 扩展测试设计明确要求先查证实现对 0 字节请求、重复释放、NULL 释放的实际约定，**对未定义行为不要在正式测试中调用**——把"非法输入测试"从断言模块契约改为澄清模块契约。

此外，提示词七记录了一条与前几条不同类型的经验：**复核阶段的提示词不能只要求"找 bug"，还必须要求"撤回不成立的批评"**。首版评审提出的若干条批评在复核后被证明不成立，若不明确撤回，后续会有人按错误结论去改本来正确的代码。

---

## 关于提示词记录范围的说明

本文件汇总的是**本次实验实际使用过的提示词**，不要求为报告的每一个章节单独配一条提示词。实验报告中的实验目的、整体逻辑分析与总结收获等章节属于对实验过程的归纳与表述，其写作依托于前述各阶段提示词所积累的结论，不需要另立提示词条目。

---

### Prompt T1-1 —— 实现 `page_init()`（主实现）

> 对应任务：T1：实现 `page_init()`

**使用场景：** 拿到 ucore riscv64 Lab2 骨架后，实现 `kern/mm/pmm.c` 中的 `page_init()`。这是本任务的主体工作。

```markdown
[PROMPT]
我正在做 riscv64-ucore 系统的 Lab2「物理内存和页表」，需要实现 kern/mm/pmm.c 中的 page_init() 函数。

【做什么】
page_init() 负责建立物理页描述符，并把可用页面交给物理内存管理器：
1. 根据设备树提供的物理内存信息建立可管理物理页的描述符布局。
2. 把排除 OpenSBI、内核镜像和描述符数组占用空间后的完整空闲页面交给当前 pmm_manager 管理，为后续页面分配提供正确的初始状态。
3. page_init() 不接收参数也不返回结果，由 pmm_init() 调用。
4. 成功返回时应正确设置 va_pa_offset、npage 和 pages，输出探测到的物理内存范围，并完成当前物理内存管理器初始空闲区的注册。

【改哪里】
只改 kern/mm/pmm.c 中的 page_init() 函数体。其他设置以及声明一律不动：不新增函数、不修改 pmm.c 中的其他函数、不修改任何其他文件。

【怎么改】
1. 必须区分虚拟地址、物理地址、物理页号和 Page 描述符地址，四者不可混用。
2. 正确处理可管理上界和页面对齐。
3. 确保内核及描述符数组不会进入空闲区。
4. 设备树信息无效时应终止初始化（panic）。
5. 没有可用完整页面时不得调用 init_memmap()。
6. 可使用：get_memory_base()、get_memory_size()、panic()、cprintf()、init_memmap()、pa2page()、链接符号 end、struct Page 及相关全局变量。
7. 使用框架提供的内存上界、地址转换、页面对齐和页面状态宏完成初始化，不要自己另写一套等价实现。

[RELY]
框架已提供下列能力，直接用，不要重复实现：

- kern/mm/pmm.h：
  - PADDR(kva)：内核虚拟地址 → 物理地址，行为 kva - va_pa_offset；传入 < KERNBASE 的地址会 panic。
  - pa2page(pa)：物理地址 → Page 描述符地址，行为 &pages[PPN(pa) - nbase]；内置越界检查 PPN(pa) >= npage 时 panic。
  - page2pa(page)、page2ppn(page)：Page 描述符地址 → 物理地址 / 物理页号。
  - extern 全局：struct Page *pages、size_t npage、const size_t nbase、uint64_t va_pa_offset。

- kern/mm/memlayout.h：
  - PGSIZE = 4096；DRAM_BASE = 0x80000000；nbase = DRAM_BASE / PGSIZE。
  - struct Page { int ref; uint64_t flags; unsigned int property; list_entry_t page_link; }
  - 页面状态：PG_reserved / PG_property 标志位，以及 SetPageReserved / ClearPageReserved / PageReserved 宏。

- libs/defs.h：ROUNDUP(a, n) / ROUNDDOWN(a, n)，基于 GNU 语句表达式（需 -std=gnu99）。

- kern/driver/dtb.c：get_memory_base() / get_memory_size()，返回设备树报告的物理内存基址与大小。

- kern/mm/default_pmm.c：default_init_memmap() 以 assert(PageReserved(p)) 开头，随后清空该页的 flags 与 ref。也就是说——交给 pmm_manager 的页必须已被标记为 reserved，由管理器自己负责清除标记。

物理内存布局前提（低 → 高）：
  [ OpenSBI (DRAM_BASE 之下) ][ 内核镜像 (0x80200000 起) ][ 空闲页 ... ]
链接符号 end 指向内核镜像 .bss 的末尾，是内核占用的物理上界。

pmm_init() 的调用顺序固定为：
  init_pmm_manager()  →  page_init()  →  check_alloc_page()
即 page_init() 必须把空闲区注册进当前的 pmm_manager（通过 init_memmap()），check_alloc_page() 紧随其后执行。

[GUARANTEE]
【需要交付的函数列表】
1. void page_init(void) —— 实现于 kern/mm/pmm.c。这是本任务唯一需要交付的函数。
2. 不需要交付任何其他函数。不要新增辅助函数，不要修改 pmm.c 中已有的 init_pmm_manager()、init_memmap()、alloc_pages()、free_pages()、nr_free_pages()、pmm_init()、check_alloc_page()。
3. 不修改任何其他文件（不新建 .c/.h，不改 Makefile，不改 libs/、kern/driver/ 等）。

【硬性底线】以下每一条都必须成立，任一不成立即视为未完成：
1. 绝不越界写：所有地址换算一律走 PADDR() / pa2page() / PPN() / ROUNDUP()，不手写 &pages[PPN(x)-nbase]、kva - va_pa_offset、位运算取整等等价表达式。
2. npage 一律取 PPN(mem_base + mem_size)，**不得**用编译期常量 PHYSICAL_MEMORY_END 截断或替换它（DTB 报告的内存大于编译期上界时，截断会导致与真实探测结果不一致）。
3. OpenSBI、内核镜像、描述符数组三者占用的物理页绝不进入交给 pmm_manager 的空闲区。
4. 没有可用完整页面时，绝不调用 init_memmap()。
5. 设备树信息无效时必须 panic，且 panic 之前不得对 pages / npage / 描述符数组做任何写入。
6. 设备树信息有效但没有完整页时，正常返回（不 panic），且不调用 init_memmap()。
7. 在 -std=gnu99 -Wall -Werror 下零警告通过。

[SPECIFICATION]
函数签名：void page_init(void)（无参数、无返回值）

【前置条件】
- dtb_init() 已完成，get_memory_base() / get_memory_size() 可返回设备树报告值。
- pmm_manager 已由 init_pmm_manager() 选定并完成其 init()。
- 全局 pages / npage / va_pa_offset 尚未被本函数写入。

【执行步骤与失败出口】
步骤 1  va_pa_offset = PHYSICAL_MEMORY_OFFSET
步骤 2  mem_base = get_memory_base()；mem_size = get_memory_size()
        失败出口：mem_base == 0 || mem_size == 0 → panic("page_init: invalid physical memory info from device tree")
步骤 3  mem_end = mem_base + mem_size
        失败出口：mem_end < mem_base（回绕） → panic("page_init: physical memory range overflows")
步骤 4  npage = PPN(mem_end)
        失败出口：npage <= nbase → panic("page_init: physical memory range is too small")
步骤 5  n_desc = npage - nbase
步骤 6  end_va = (uint64_t)end；end_pa = (uint64_t)PADDR(end_va)
步骤 7  start_va = ROUNDUP(end_va, PGSIZE)；start_pa = ROUNDUP(end_pa, PGSIZE)
步骤 8  array_bytes = ROUNDUP(n_desc * sizeof(struct Page), PGSIZE)
        失败出口：start_pa + array_bytes > mem_end → panic("page_init: no room left for the page descriptor array")
步骤 9  free_start_pa = start_pa + array_bytes
步骤 10 pages = (struct Page *)start_va
步骤 11 从 pa2page(DRAM_BASE) 起，对物理区间 [DRAM_BASE, start_pa) 的每一页调用 SetPageReserved
        （该区间同时覆盖 OpenSBI 与内核镜像）
步骤 12 从 pa2page(start_pa) 起，对物理区间 [start_pa, free_start_pa) 的每一页调用 SetPageReserved
        （该区间是描述符数组自身）
步骤 13 n_free = npage - PPN(free_start_pa)
        n_free > 0  → 从 pa2page(free_start_pa) 起，对 [free_start_pa, mem_end) 的每一页调用 SetPageReserved，
                      然后调用 init_memmap(pa2page(free_start_pa), n_free)
        n_free == 0 → **不调用** init_memmap()，打印 "page_init: no free physical page left"
步骤 14 打印三行启动输出（格式见「交付形态」）

【顺序约束】npage 必须在任何 pa2page() 调用之前完成赋值——pa2page() 的越界检查依赖 npage，npage 尚未赋值时任何调用都会误判越界并 panic。

【后置条件】（成功返回时以下全部成立）
1. va_pa_offset == PHYSICAL_MEMORY_OFFSET
2. npage == PPN(mem_base + mem_size)，且 npage > nbase
3. pages == (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE)，且该地址页对齐
4. 描述符数组元素个数 == npage - nbase；元素 i 描述物理页号 nbase + i
5. 物理页号 < nbase 的全部页（OpenSBI）已 SetPageReserved
6. 物理页号从 nbase 到 PPN(start_pa) - 1（内核镜像，含 end 之前）已 SetPageReserved
7. 描述符数组自身占用的页已 SetPageReserved
8. init_memmap() 被调用且仅被调用一次（或在无可用完整页时 0 次），传入的区间是描述符数组之后的第一个页对齐物理地址起、到 mem_end 止的完整页
9. 串口已打印物理内存 base / size / end、描述符数组 va / pa / 页数、空闲页数与 npage

【失败出口总则】每一步的异常路径只能且只能是 panic 或「跳过 init_memmap 继续」，不得有第三条路径，不得静默返回。

【交付形态】
1. 只输出 page_init() 的完整 C 代码（含必要注释），不要输出其他文件的改动，不要给出 diff。
2. 代码之后，附一张表格说明四个地址概念在本实现中的具体对应关系：虚拟地址（内核虚拟地址 / 描述符数组虚拟地址）、物理地址、物理页号（PPN）、Page 描述符地址。每行给出：概念、取值来源、本实现中用到的宏或表达式。
3. 附上启动期应打印的 cprintf 输出格式示例（至少包含：探测到的物理内存 base/size/end、描述符数组的虚拟地址与物理地址与页数、空闲页数与 npage），用占位符表示运行时数值。
4. 单独列出 panic 的全部触发条件，每条写明触发时机与触发原因。
```

---

### Prompt T1-2 —— 重构：改用框架入口，不手写等价表达式

> 对应任务：T1：实现 `page_init()`

**使用场景：** T1-1 的实现把 `pa2page()` 的函数体当普通表达式抄了一遍（`&pages[PPN(x) - nbase]`），丢掉了它内置的越界检查。要求重构。

```markdown
[PROMPT]
请重构你刚才给出的 page_init() 实现。

【做什么】
只做等价重构，不改变任何行为：把 page_init() 中所有手写的地址换算表达式换成框架提供的函数与宏。

【改哪里】
仍然只改 kern/mm/pmm.c 中 page_init() 的函数体，不动其他任何位置。

【怎么改】
1. 所有涉及「物理地址 ↔ Page 描述符地址」转换的地方，一律调用框架的 pa2page()，不要手写 &pages[PPN(x) - nbase] 这样的等价表达式。
2. 所有涉及「内核虚拟地址 → 物理地址」的地方，一律调用框架的 PADDR()，不要手写 kva - va_pa_offset。
3. 页面对齐一律使用 libs/defs.h 的 ROUNDUP() / ROUNDDOWN()，不要手写位运算。

原因：pa2page() 内部带有 PPN(pa) >= npage 的越界检查并会在越界时 panic；手写等价表达式会静默丢掉这层护栏，在边界输入下会越界访问描述符数组而不是明确报错。同理，PADDR() 会对小于 KERNBASE 的非法虚拟地址 panic，手写减法会静默产生错误物理地址。

重构后必须保证与重构前行为完全等价：同样的输入产生同样的输出、同样的副作用、被标记 reserved 的页集合完全一致。

[RELY]
- pa2page(pa) 定义于 kern/mm/pmm.h：
      static inline struct Page *pa2page(uintptr_t pa) {
          if (PPN(pa) >= npage) { panic("pa2page called with invalid pa"); }
          return &pages[PPN(pa) - nbase];
      }
- PADDR(kva) 定义于 kern/mm/pmm.h：
      #define PADDR(kva) ({ uintptr_t __m_kva = (uintptr_t)(kva); \
          if (__m_kva < KERNBASE) { panic("PADDR called with invalid kva %08lx", __m_kva); } \
          __m_kva - va_pa_offset; })
- ROUNDUP / ROUNDDOWN 定义于 libs/defs.h，基于 GNU 语句表达式与 typeof。
- 其余依赖与 T1-1 相同：struct Page、PG_reserved / PG_property、SetPageReserved / PageReserved、get_memory_base() / get_memory_size()、init_memmap()、链接符号 end、pages / npage / nbase / va_pa_offset。

[GUARANTEE]
【需要交付的函数列表】
1. void page_init(void) —— 重构后的完整实现，仍在 kern/mm/pmm.c。这是本段唯一需要交付的函数。
2. 不需要交付任何其他函数，不修改 pmm.c 中的其他函数，不修改任何其他文件。

【硬性底线】
1. 行为与重构前逐字节等价：输出内容、副作用、被标记 reserved 的页集合完全一致。
2. 不丢失任何一处越界检查能力——凡是用 pa2page() / PADDR() 的位置，都必须保留它们内置的 panic 检查。
3. 改动范围严格限于 page_init() 函数体。
4. 在 -std=gnu99 -Wall -Werror 下零警告通过。

[SPECIFICATION]
函数签名：void page_init(void)（无参数、无返回值）

【前置条件】与 T1-1 相同。
【后置条件】与 T1-1 相同——重构不得改变任何一条后置条件。特别地，下列三条必须保持成立：
  - va_pa_offset == PHYSICAL_MEMORY_OFFSET
  - npage == PPN(mem_base + mem_size)，且未被 PHYSICAL_MEMORY_END 截断
  - init_memmap() 的调用区间与调用次数与重构前一致

【本次重构的具体规格】
1. 至少覆盖三处 pa2page() 调用，且必须恰好是这三处：
   a) OpenSBI / 内核镜像保留区的起点：pa2page(DRAM_BASE)
   b) 描述符数组保留区的起点：pa2page(start_pa)
   c) 交给 pmm_manager 的空闲区起点：pa2page(free_start_pa)
2. 至少覆盖一处 PADDR() 调用：内核镜像末尾 end 的虚拟地址 → 物理地址。
3. 至少覆盖两处 ROUNDUP()：描述符数组起点的虚拟地址与物理地址、描述符数组占用字节数的向上取整。
4. 不得残留任何形如 &pages[PPN(...)-nbase]、(... - va_pa_offset)、(... & (PGSIZE-1)) 的手写换算表达式。
5. 除上述等价替换外，不得调整任何语句的执行顺序、条件判断、循环边界或输出内容。

【交付形态】
1. 输出重构后的完整 page_init() 函数代码。
2. 随后用表格列出每一处替换点：「原手写表达式」→「改为调用的框架函数」→「因此恢复了什么检查」。
```

---

### Prompt T1-3 —— 约束与接口逐条自查复核

> 对应任务：T1：实现 `page_init()`

**使用场景：** 实现完成后，对照实验要求和评分关注点做一次逐条自查，避免漏项。本段是**复核类** prompt，不产出实现代码。

```markdown
[PROMPT]
请对你实现的 page_init() 做一次逐条自查复核。下方列出了这个函数必须满足的全部约束。

对每一条，请给出：
  - 判定：满足 / 不满足 / 部分满足
  - 依据：具体到代码行，引用相关表达式或语句
  - 若不满足或部分满足：给出最小修改方案（只改必要的那几行）

不要笼统地说"已实现所有要求"，每条都要单独核对并引用代码位置。

另请额外回答：
  a) 如果设备树报告的内存大小不是页对齐的倍数，描述符数组的页数和空闲区起点分别是多少？
  b) 如果设备树信息无效，panic 发生在哪一行？在此之前有没有任何对 pages / npage / 描述符数组的写入？
  c) 描述符数组自身的物理页是否可能出现在 init_memmap() 交给 pmm_manager 的区间里？

[RELY]
约束清单（逐条核对对象）：
1. va_pa_offset = PHYSICAL_MEMORY_OFFSET，va = pa + va_pa_offset。
2. npage = PPN(mem_base + mem_size)，以 DTB 为准，不被 PHYSICAL_MEMORY_END 截断，且 npage > nbase。
3. pages 指向页对齐地址，且位于内核镜像（end 符号）之后。
4. 描述符数组的元素个数 = npage - nbase；页下标 = PPN(pa) - nbase。
5. OpenSBI + 内核镜像 + 描述符数组三者占用的页全部 PageReserved。
6. 交给 pmm_manager 的空闲区是完整页，且不含上述三者。
7. 空闲区的页已预先标记 PageReserved（default_init_memmap 的 assert(PageReserved(p)) 依赖这一点）。
8. DTB 无效时 panic；无完整页时不调用 init_memmap()。
9. 输出探测到的物理内存范围。
10. 只改了 page_init()。

框架事实：PADDR / pa2page / PPN / ROUNDUP 的定义与行为；struct Page 的四个字段；PG_reserved 与 SetPageReserved / PageReserved；get_memory_base() / get_memory_size()；default_init_memmap() 开头的 assert(PageReserved(p))；链接符号 end 指向内核 .bss 末尾。

[GUARANTEE]
【需要交付的产物列表】
1. 一份逐条约束核对表——覆盖上面 10 条约束，一表 10 行，不合并、不省略。
2. 三段追加回答，分别对应额外问题 a、b、c。

【判定标准】
1. 每条约束单独一行判定并引用具体代码位置，不得合并回答、不得用"已全部实现"之类笼统表述。
2. 判定为「不满足」或「部分满足」时，必须同时给出可编译的最小修改方案；只满足于文字描述不算。
3. 额外问题 a/b/c 必须给出具体数值或具体代码行，不得只给原则性描述。
4. 本段是复核，不产出实现代码：你不得直接改写 page_init()，只输出核对结论与修改建议。

[SPECIFICATION]
核对表字段规格：
  列 1  约束编号（1–10，取自 [RELY] 的约束清单）
  列 2  判定（满足 / 不满足 / 部分满足，三选一，不得留空、不得写其他值）
  列 3  依据（引用 page_init() 中的具体表达式或语句，定位到代码行）
  列 4  修改建议（判定为满足时填"—"；判定为不满足/部分满足时填可编译的最小修改）

追加回答的字段规格：
  问题 a  回答三件事：描述符数组的页数（公式 + 数值）、空闲区起点物理地址、为什么要先向上取整
  问题 b  回答两件事：panic 发生的代码位置、panic 之前对 pages / npage / 描述符数组是否发生过写入（是/否 + 依据）
  问题 c  回答两件事：会/不会 + 依据（说明描述符数组区间与 init_memmap 区间在物理地址上如何分离）
```

---

### Prompt T3-2 —— 针对已实现的 Buddy System 模块设计并生成测试函数（测试 prompt）

> 对应任务：T3 Buddy System 的第二份 prompt（测试用）

**使用场景：** 刘婕的第一份 prompt 已经生成了 Buddy System 的实现（`kern/mm/buddy_pmm.c/h`）并接入了内核，但 **Buddy System 在本实验中没有框架自带的测试函数**（不像 `best_fit_pmm.c` 那样有现成的 `best_fit_check()`）。需要针对已生成的实现，自主设计一套测试方案并生成测试代码，用来验证这个分配器在各种请求形态和边界条件下是否正确。

```markdown
[PROMPT]
我在做 riscv64-ucore 的 Lab2（物理内存和页表）。Buddy System 分配器的实现已经由另一份 prompt 生成并接入内核，代码在 kern/mm/buddy_pmm.c 与 kern/mm/buddy_pmm.h（配套的实现自检在 kern/mm/buddy_test.c）。

现在请你为这个 Buddy System **设计并生成一套测试函数**，生成到新文件 kern/mm/buddy_verify.c 与 kern/mm/buddy_verify.h。

为什么必须另写一套，而不是直接用已有的 buddy_test.c：
1. 已有的自检用的是裸 assert，失败会直接 panic 内核，**一条用例挂掉就看不到后面所有用例的结果**。我要的是"全部用例跑完，再给出一份失败清单"。
2. 我需要的是**外部验证**：站在"使用者"的角度调用 buddy 的公开接口，检查它对外表现出的性质，而不是复述它的内部实现。
3. 测试必须是**可判定**的：每一条检查都要有明确的"期望值 vs 实际值"，失败时打印出是哪一个性质被违反。

请按下面的要求做。

（1）测试风格：计数式检查，不要用 assert。
    - 实现两个全局计数器：vt_pass（通过的检查条数）、vt_fail（失败的检查条数）；
    - 实现一个检查函数，例如 cprintf 风格的 vt_check(int cond, const char *what, unsigned long expect, unsigned long actual)：
      条件为真则 vt_pass++ 并打印一行 [verify]   ok: <描述>；
      条件为假则 vt_fail++ 并打印一行 [verify]   FAIL: <描述>  (expect <期望>, actual <实际>)；
    - 一条失败**绝不能**让内核停机，也绝不能影响后续用例的执行。

（2）测试框架：分 10 组用例，每组是一个返回 int（0 表示本组通过）的函数，用一个统一的运行器 vt_run(const char *name, fn) 依次调用。
    运行器每组打印 [verify] -- <组名>，跑完打印 [verify] PASS <组名> 或 [verify] FAIL <组名>；
    全部跑完后打印一行汇总：
        [verify] summary: <通过组数>/<总组数> groups passed, <通过检查数>/<总检查数> checks passed
    并在全部检查通过时打印 [verify] RESULT: buddy verify PASS，否则打印 [verify] RESULT: buddy verify FAIL。

（3）必须分成 10 组用例来覆盖，每组至少 3 条独立检查：
    V1 恰好 2 的幂的请求；V2 非 2 的幂的请求；V3 拆分覆盖性；V4 合并回填；V5 不得误合并；
    V6 往返一致性；V7 非法释放被安全拒绝；V8 按阶分布守恒；V9 边界与耗尽；V10 与 ucore 框架的对接。
    每组具体要断言哪条性质，见下面 [SPECIFICATION]。

（4）每组用例结束时，必须检查"总量守恒"：本次测试分配出去的页数与已释放的页数必须配平，不允许有页泄漏。用一个本文件内私有的"在途页数"计数器来核对。

（5）测试代码里禁止使用框架自带的 assert；禁止修改 buddy_pmm.c / buddy_pmm.h / buddy_test.c / pmm.h / memlayout.h 任何一个文件；测试代码只允许调用 buddy_pmm.h 里已经声明的公开接口，不得直接访问 buddy 模块的内部静态变量。

（6）代码风格与仓库一致：GNU99，4 空格缩进，函数返回类型与参数类型分行写（与仓库既有风格一致），每个文件头写明作者与用途的注释。所有新增文件必须能在 -Wall -Werror 下零警告编译通过。

（7）最后给我一段文字说明：这 10 组用例各自检验了 Buddy System 的哪一条定义性性质、为什么这样检验能抓住实现错误、以及哪些性质属于"实现细节"因而不适合作为测试判据。

[RELY]
以下代码与框架已经存在，**可以直接使用，不要重新实现，也不要修改**：

1. Buddy System 的公开接口（在 kern/mm/buddy_pmm.h 中已声明，直接 include <buddy_pmm.h> 即可）：
    void          buddy_init(void);                        /* 初始化管理池 */
    struct Page *buddy_alloc_pages(size_t n);              /* 分配 n 页（向上取整到 2 的幂）*/
    void          buddy_free_pages(struct Page *base, size_t n);
    int           buddy_free_pages_checked(struct Page *base, size_t n);  /* 非法释放返回非 0，不 panic */
    size_t        buddy_nr_free_pages(void);                /* 空闲页总数 */
    size_t        buddy_nr_free_blocks(int order);          /* 某一阶的空闲块数 */
    size_t        buddy_pool_npages(void);                  /* 管理池的总页数 */
    size_t        buddy_page_offset(struct Page *p);         /* 页在管理池内的偏移（页号）*/
    以及两个常量：#define BUDDY_MAX_ORDER 12、#define BUDDY_NR_ORDER (BUDDY_MAX_ORDER + 1)

2. 框架侧可用的东西：
    - kern/mm/memlayout.h：struct Page、PG_reserved/PG_property、pa2page()、page2pa()、PGSIZE；
    - kern/mm/pmm.h：全局 npage（物理内存总页数）、pmm_manager 的 alloc_pages/free_pages；
    - kern/stdio.h：cprintf()（内核里没有 printf）；
    - kern/mm/buddy_test.c：已有的实现自检（7 个子测试，用裸 assert），只可阅读参考，**不得修改、不得在其中追加代码**。

3. 关键事实（已核实，测试的前提）：
    - buddy_init() 必须由调用方在 pmm_init() 之后调用，因为它要向底层申请管理池；
    - 管理池是物理连续的 4096 页；块在池内始终是一个连续的偏移区间；
    - buddy_free_pages_checked() 是"安全版释放"，非法释放会被拒绝并返回非 0，而不会 panic。

[GUARANTEE]
1. 新增且只新增两个文件：kern/mm/buddy_verify.c 与 kern/mm/buddy_verify.h；除这两个文件外，**一个字节都不许改**。
2. 测试代码中不得出现 assert / panic；任何一条检查失败都只计数并打印，**内核必须活着跑完 10 组用例**。
3. 全部 10 组用例必须执行到（不允许提前 return），并打印出规定的 summary 与 RESULT 两行。
4. 每个新增文件都能在 -std=gnu99 -Wall -Werror 下零警告编译通过。
5. 测试只使用 buddy_pmm.h 已声明的公开接口与框架的公开接口，不访问被 static 修饰的内部变量。
6. 每一组用例结束时，测试内部记录的在途页数必须回到 0（不允许泄漏）。
7. 不得因为"某个用例失败"而吞掉后续用例；失败信息必须包含：是哪条性质、期望值、实际值。

[SPECIFICATION]
一、文件与入口
  - kern/mm/buddy_verify.h：只声明一个函数 void buddy_verify(void);，加文件头注释说明用途与作者。
  - kern/mm/buddy_verify.c：实现全部检查逻辑与上面的入口。

二、检查原语
  - 全局计数器：unsigned int vt_pass（通过的检查条数）、unsigned int vt_fail（失败的检查条数）。
  - void vt_check(int cond, const char *what, unsigned long expect, unsigned long actual)
      cond 为真：vt_pass++，打印 "[verify]   ok: <what>"
      cond 为假：vt_fail++，打印 "[verify]   FAIL: <what>  (expect <expect>, actual <actual>)"
      **任何情况下都不许停机、不许 assert。**
  - private 的"在途页数"计数器：每次 buddy_alloc_pages 成功就加上该块实际占用的页数，
    每次 buddy_free_pages 成功就减去相应页数；每组用例结束时检查它是否回到 0。

三、用例组运行器
  - typedef int (*vt_test_fn)(void);  每组是一个返回 int 的函数（0 = 本组通过）。
  - void vt_run(const char *name, vt_test_fn fn)
      进入时打印 "[verify] -- <name>"；执行后若本组产生了任何失败则该组判为失败；
      结束时打印 "[verify] PASS <name>" 或 "[verify] FAIL <name>"。
  - buddy_verify() 依次调用 10 组，最后打印：
      "[verify] summary: <通过组数>/<总组数> groups passed, <通过检查数>/<总检查数> checks passed"
      全部检查通过时再打印 "[verify] RESULT: buddy verify PASS"，否则打印 "[verify] RESULT: buddy verify FAIL"。

四、10 组用例各自必须断言的性质（每组至少 3 条独立检查）
  V1 恰好 2 的幂的请求
     - 申请 n = 2^k，返回的块的实际阶数必须恰好是 k（用 buddy_nr_free_blocks(k) 的变化或块大小验证），不多给；
     - 分配后第 k 阶的空闲块计数减少 1，第 k+1 阶不变；
     - 分配成功后 buddy_nr_free_pages() 恰好减少 n。
  V2 非 2 的幂的请求
     - 申请 n（不是 2 的幂）时，实际分配的块大小必须等于向上取整到的 2 的幂；
     - 计费与取整后的大小一致（空闲页数减少的是取整后的页数）；
     - 连续申请多个非 2 的幂请求，各自取整到正确的阶。
  V3 拆分覆盖性
     - 取一个 2^(k+1) 的块，拆分后两个 2^k 半块的**覆盖区间合起来恰好等于原块**（用 buddy_page_offset 计算区间端点比对，不多一页不少一页）；
     - 两个半块互不重叠；
     - 两个半块是伙伴关系（偏移的对应位异或后互为伙伴）。
  V4 合并回填
     - 释放其中一个半块后，它必须与伙伴合并成 2^(k+1) 的整块；
     - 合并后该阶的空闲块计数回到分配前的值；
     - 合并后的块大小等于被合并前那个整块的大小。
  V5 不得误合并
     - 构造两个"物理相邻但不是伙伴"的块，释放其中一个后，**另一个必须仍然独立存在**（其阶数、地址、块数都不变）；
     - 被释放的块不得与它合并成一个更大的块；
     - 释放后各阶的空闲块计数变化符合"只释放了自己"这一预期。
  V6 往返一致性
     - 一轮"分配若干块 → 全部释放"之后，各块互不重叠；
     - 释放回来的空闲页总数与分配前一致；
     - 各阶的空闲块计数与分配前逐阶一致。
  V7 非法释放被安全地拒绝
     - 重复释放同一个块：必须被拒绝（用 buddy_free_pages_checked 返回非 0 判定），且不 panic；
     - 释放一个不是块首的页：必须被拒绝；
     - 释放管理池之外的地址：必须被拒绝；
     - 三次非法释放之后，管理器的状态（空闲页数、各阶块数）与非法释放之前**完全一致**。
  V8 按阶分布守恒
     - 一轮"分配若干不同阶的块 → 全部释放"之后，**每一个阶**的空闲块计数都必须回到分配之前的值；
     - 空闲页总数守恒；
     - 在途页数回到 0。
  V9 边界与耗尽
     - n = 0：必须返回 NULL 或以明确方式拒绝，不得返回非法指针、不得挂死；
     - n 超过管理池容量：必须返回 NULL，不得越界、不得挂死；
     - 反复申请直到耗尽：耗尽后必须返回 NULL（可预期的失败），不得死循环；
     - 池耗尽过程中不得 panic。
  V10 与 ucore 框架的对接
     - 管理池的总页数 * PGSIZE 必须落在物理内存范围内（用 npage 判定）；
     - 同一个块内的各页必须**物理连续**（逐页比对 page2pa 的相邻性）；
     - 三个查询接口（空闲页总数、各阶块数、池大小）的返回值必须与实际状态一致；
     - 测试中分配出去的块，其物理地址必须能被 pa2page() 正确映射回描述符。

五、输出格式（必须逐字一致，便于脚本/截图核对）
  [verify] ===== buddy verify start (Li Sihan 2512163) =====
  [verify] -- <组名>
  [verify]   ok: <描述>
  [verify]   FAIL: <描述>  (expect <期望>, actual <实际>)
  [verify] PASS <组名> / [verify] FAIL <组名>
  [verify] summary: <通过组数>/<总组数> groups passed, <通过检查数>/<总检查数> checks passed
  [verify] RESULT: buddy verify PASS / [verify] RESULT: buddy verify FAIL
  [verify] ===== buddy verify end =====

六、交付时的文字说明
  逐组说明：这组检验的是 Buddy System 的哪一条**定义性性质**（正确实现必然满足、错误实现必然违反）、
  为什么这样检验能抓住实现错误、以及哪些性质属于内部实现细节、因而不适合作为测试判据。
```

---
