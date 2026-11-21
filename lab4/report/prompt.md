# Lab4 提示词汇总

本文件汇总 Lab4 全部分工使用的四段式提示词（`[PROMPT]` / `[RELY]` / `[GUARANTEE]` / `[SPECIFICATION]`），以及每个模块的真实迭代记录。

| 段 | 作用 |
|---|---|
| `[PROMPT]` | 让模型做什么：任务、边界、输出形态 |
| `[RELY]` | 模型可以依赖的事实：只给仓库里真实存在的接口与语义，不给记忆里的"经典实现" |
| `[GUARANTEE]` | 验收条件：逐条可检查的交付物与不变式，越界即不合格 |
| `[SPECIFICATION]` | 行为契约：前置条件、各分支的精确语义、后置条件与可观察判据 |

四个模块的分工：

| 模块 | 负责人 | 代码位置 |
|---|---|---|
| 阶段 0：lab2/lab3 回填 | 2512130-刘婕 | `kern/mm/default_pmm.c`、`kern/mm/pmm.c`、`kern/trap/trap.c`、`kern/trap/trapentry.S` |
| 模块一：虚拟内存页操作 | 2512163-李思翰 | `kern/mm/pmm.c`（`//LAB4 function1`） |
| 模块二：进程创建与初始化 | 2512436-郭睿彤 | `kern/process/proc.c`（`//LAB4 function2`） |
| 模块三：调度与上下文切换 | 2512130-刘婕 | `kern/process/proc.c`（`//LAB4 function3`）、`kern/schedule/sched.c`、`kern/process/switch.S` |

扩展练习 Challenge 共四道，全部是分析题、不产出代码，因此**没有四段式提示词**：
`[GUARANTEE]` 与 `[SPECIFICATION]` 这两段在"解释既有代码"的场景里退化成空话，
反而失去约束力。这四题的回答方式与分工见 `report.md` 第四章「扩展练习 Challenge」。

---

## 提示词零：lab2/lab3 代码回填（2512130-刘婕）

### [PROMPT]

```
在 lab4 骨架中恢复前三个实验的成果，使工程重新具备"能启动到 cpu_idle"的底座。
lab4 骨架相比 lab3 删掉的不只是函数体，还包括若干文件与调用点，需要逐项补齐：

  1. kern/mm/best_fit_pmm.h —— 骨架缺失，但 kern/mm/default_pmm.c 第 3 行 include 了它；
  2. kern/mm/default_pmm.c —— 补 Best-Fit 分配器的五个函数与 default_pmm_manager 表；
  3. kern/mm/pmm.c 的 page_init() —— 物理内存探测与 struct Page 数组建立；
  4. kern/trap/trap.c 的 interrupt_handler() / exception_handler()；
  5. kern/trap/trapentry.S 的 RESTORE_ALL 与 __alltraps 中的 call trap。

【约束】
- 骨架自带的 basic_check() / best_fit_check() 已在 default_pmm.c 末尾，只补分配器与
  manager 表，不要连 check 一起搬（会重复定义）；
- 不得把测试文件拷进 kern/test/，不得在 Makefile 或 init.c 里出现 test_lab4；
- 每处回填的注释标注改为「lab4 backfill 2512130」或保留原 labN 标记 + 实际学号。
```

### [RELY]

**lab4 骨架与 lab3 的差异（`diff -rq` 实测）**

```
Only in lab3/code/kern/mm: best_fit_pmm.c
Only in lab3/code/kern/mm: best_fit_pmm.h
Files lab3/code/kern/mm/default_pmm.c and lab4/code/kern/mm/default_pmm.c differ
Files lab3/code/kern/mm/pmm.c and lab4/code/kern/mm/pmm.c differ
Files lab3/code/kern/trap/trap.c and lab4/code/kern/trap/trap.c differ
Files lab3/code/kern/trap/trapentry.S and lab4/code/kern/trap/trapentry.S differ
```

lab4 的 `kern/mm/` 只有 `default_pmm.c/h`、`kmalloc.c/h`、`memlayout.h`、`mmu.h`、`pmm.c/h`、`vmm.c/h`，
**没有** `best_fit_pmm.c/h`、`buddy_pmm.*`、`slub.*`。

**`default_pmm.c` 的真实结构（lab4 骨架）**

```c
#include <best_fit_pmm.h>          // 骨架 include 了，但文件不存在
...
// 骨架末尾已有：
static void basic_check(void) { ... }
static void best_fit_check(void) { ... }
```

即：Best-Fit 被塞进 `default_pmm.c`，但注册名仍叫 `default_pmm_manager`，
且两个 check 已在文件末尾 —— 回填时**只补分配器 + manager 表**。

**`pmm.c` 的真实注册点**

```c
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;   // 骨架默认值，没有任何注释提示
}
```

**`trapentry.S` 的槽位约定（lab3 已验证）**

```c
struct trapframe {
    /* 0..31  */ uintptr_t gpr[32];
    /* 32     */ uintptr_t status;      // sstatus
    /* 33     */ uintptr_t epc;         // sepc
    /* 34     */ uintptr_t badvaddr;    // stval
    /* 35     */ uintptr_t cause;       // scause
};
```

### [GUARANTEE]

| # | 交付项 | 检查方式 |
|---|---|---|
| 1 | `kern/mm/best_fit_pmm.h` 存在，且被 `default_pmm.c` 引用 | `ls kern/mm/best_fit_pmm.h` |
| 2 | `default_pmm.c` 提供 `best_fit_init/init_memmap/alloc_pages/free_pages/nr_free_pages` 与 `default_pmm_manager` | `grep -n "best_fit_" kern/mm/default_pmm.c` |
| 3 | `pmm.c` 的 `page_init()` 完成物理内存探测 | 启动打印 `free pages: ...` |
| 4 | `trapentry.S` 的 `RESTORE_ALL` 写回 4 个 CSR，且 `x2` 是最后一条访存 | 逐槽核对 |
| 5 | 无重复定义 | `make` 不报 `multiple definition` |
| 6 | 不污染骨架 | `grep -rn "test_lab4" Makefile kern/init/init.c` 无输出 |

### [SPECIFICATION]

**`RESTORE_ALL` 的契约**

- **Pre-Condition**：`sp` 指向 `struct trapframe` 的 slot 0；槽 32~35 已由 `SAVE_ALL` 填好。
- **Order**（三条硬顺序，任一违反都不报错、只跑错）：
  1. CSR 先写：`LOAD t0, 32*REGBYTES(sp)` → `csrw sstatus, t0`，33/34/35 同理。
     中转寄存器 `t0` 自身在 slot 5 里，随后会被恢复，不丢信息。
  2. 通用寄存器：`x1`、`x3`~`x31`，**不恢复 `x0`**（硬件恒 0）。
  3. `sp` 最后：`LOAD x2, 2*REGBYTES(sp)`。在它之前所有 LOAD 都以 `sp` 为基址。
- **Post-Condition**：`sret` 之后 35 个寄存器、4 个 CSR 与陷入前逐位相同。

**`interrupt_handler()` 中时钟分支的契约（lab4 与 lab3 的唯一功能差异）**

```c
case IRQ_S_TIMER:
    clock_set_next_event();          // 续约
    ticks++;
    current->need_resched = 1;       // lab4 新增：每个 tick 都请求重调度
    if (ticks % TICK_NUM == 0) print_ticks();
    break;
```

- lab3 同位置**没有**第三行：那时没有进程概念，时钟只负责计数与打印；
- lab4 若漏掉它，`cpu_idle` 的 `if (current->need_resched)` 永远不成立，
  系统停在第一次调度后的 idle 空转里，现象是"卡住不动"，编译器不报错；
- `trap.c` 必须新增 `#include <proc.h>` 才能访问 `current`，骨架里没有这一行。

**`page_init()` 的契约**

- 物理内存范围从设备树读取，不写死。
- `struct Page` 数组紧接内核镜像末尾，按页对齐。
- OpenSBI、内核镜像、描述符数组本身标 reserved，剩余整页交给分配器。
- 产出 `pages`、`npage`、`free_list`、`nr_free`（由 `pmm_manager->init_memmap` 建立）。

### 迭代记录

| 轮次 | 现象 | 原因 | 处理 |
|---|---|---|---|
| 1 | `fatal error: best_fit_pmm.h: No such file or directory` | 骨架删了头文件但保留 include | 从 lab3 拷入 `best_fit_pmm.h` |
| 2 | `multiple definition of 'basic_check'` | 把 lab3 的整个 `best_fit_pmm.c` 当 `default_pmm.c` 覆盖，连 check 一起搬了 | 只保留分配器五函数 + manager 表，骨架自带的 check 不动 |
| 3 | 启动打印仍为 `default_pmm_manager` | `pmm_manager` 结构体没了但注册点没改 | 重建函数指针表并让 `pmm_manager = &default_pmm_manager` 指向 Best-Fit 实现 |
| 4 | `kern/mm/kmalloc.c:92` 报 `error: passing argument 1 of 'kva2page' makes pointer from integer without a cast [-Wint-conversion]` | 骨架自带：`__slob_free_pages(unsigned long kva, int order)` 把整数直接传给 `kva2page(void *)`。C99 中整数与指针的隐式转换属于约束违例，GCC 14 起由警告变为**默认报错** | 在调用处显式写成 `kva2page((void *)kva)`，语义不变。**原判断"VM 内只 warning、不用改"是错的**——实测该行直接中断构建 |
| 5 | `make grade` 报 `kern/test/test_lab4.c:90: error: passing argument 1 of 'kernel_thread' from incompatible pointer type [-Wincompatible-pointer-types]`，随后三个用例全 `no $qemu_out`、`Total Score: 0/30` | 同一根因的另一处：注入的 `tools/test_lab4.c` 把 `void (*)(void *)` 交给 `kernel_thread(int (*)(void *), ...)`。**与三个模块的逻辑无关**，是编译期就停了，`bin/kernel` 根本没生成 | 测试文件（课程提供）与 `kernel_thread` 签名（实验规定）都不动，只在 `Makefile` 加 `CFLAGS += -Wno-error=incompatible-pointer-types`，把诊断降回警告而非静默关闭 |

---

## 提示词一：虚拟内存页操作（2512163-李思翰）

### [PROMPT]

```
请修改本仓库真实文件 kern/mm/pmm.c，只在被标记为
"//LAB4 function1 YOUR CODE" 的空洞处补全两个函数：

    int  page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
    void page_remove(pde_t *pgdir, uintptr_t la);

【任务】
在 RISC-V Sv39 三级页表之上实现"建立映射"与"撤销映射"两个原子操作，使得：

  1. page_insert 把物理页 page 映射到线性地址 la，权限位由 perm 指定；
  2. page_remove 撤销 la 处已有的映射，并在页引用计数归零时释放物理页；
  3. 页表项（PTE）、struct Page 的引用计数、TLB 三者在操作完成后始终一致；
  4. 两个函数都要能安全处理"地址原本没有映射"的情况：
     page_insert 正常建立映射，page_remove 安静返回。

【对外接口（实现必须满足，不得改变其签名与语义）】

  int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
      - 成功返回 0；
      - get_pte(pgdir, la, 1) 返回 NULL（中间级页表分配失败）时返回 -E_NO_MEM；
      - 成功后：la 对应的叶子 PTE 有效，指向 page，权限位按 perm 设置；
      - 无论原本有没有映射、映射的是不是同一个页，都必须保持引用计数正确。

  void page_remove(pde_t *pgdir, uintptr_t la);
      - 无返回值；
      - la 没有映射时是幂等的空操作；
      - 有映射时：清空叶子 PTE -> 失效 TLB -> 旧页引用计数减一 -> 归零才释放。

【操作要求】
- 必须直接修改仓库里的真实文件 kern/mm/pmm.c，不要新建替代文件，
  不要只给代码片段或伪代码；
- 只填 //LAB4 function1 这一处空洞，不得修改 get_pte、tlb_invalidate、
  check_pgdir、check_boot_pgdir、pmm_init 等既有代码；
- 不得修改 kern/mm/pmm.h、kern/mm/mmu.h、kern/mm/memlayout.h 的既有定义；
- 必须使用仓库已有的辅助函数/宏：get_pte、pte_create、pte2page、page2ppn、
  page_ref_inc、page_ref_dec、free_page、tlb_invalidate，以及 PTE_V；
- 注释用中文说明"为什么这样写"，不要写"第一步做了什么"式的流水账；
- 保留 //LAB4 function1 标记，把 YOUR CODE 替换为本人学号 2512163。

【输出要求】
- 给出 page_insert 和 page_remove 的完整函数体；
- 单独列出一张表，说明 page_insert 在"无映射 / 同页重映射 / 换页覆盖"
  三种情况下 page->ref 与空闲页数的变化；
- 说明失败路径（get_pte 返回 NULL）下为什么不会留下半完成映射；
- 给出至少一条可观察的正确性判据（不能只说"跑一下不崩"）。
```

### [RELY]

**目标函数的声明（`kern/mm/pmm.h`）**

```c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
```

**`get_pte` 的真实行为（既有实现，不可修改）**

```c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
            return NULL;
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
            return NULL;
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}
```

要点：`create=1` 按需分配中间级页表页，失败返回 `NULL`；`create=0` 只查找不分配；
返回值是**叶子 PTE 的内核虚拟地址**；中间级页表页也被 `set_page_ref(page, 1)` 计入引用。

**地址与页表项相关宏（`kern/mm/mmu.h`）**

```c
#define PDX1(la)  ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)   /* 30 */
#define PDX0(la)  ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)   /* 21 */
#define PTX(la)   ((((uintptr_t)(la)) >> PTXSHIFT)  & 0x1FF)   /* 12 */
#define PTE_ADDR(pte) (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
#define PTE_V 0x001
#define PTE_R 0x002
#define PTE_W 0x004
#define PTE_X 0x008
#define PTE_U 0x010
#define PTE_PPN_SHIFT 10
```

**页描述符与引用计数辅助函数**

```c
static inline ppn_t page2ppn(struct Page *page);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pte2page(pte_t pte);    /* 内部 assert(PTE_V) */
static inline int page_ref(struct Page *page);
static inline int page_ref_inc(struct Page *page) { page->ref += 1; return page->ref; }
static inline int page_ref_dec(struct Page *page) { page->ref -= 1; return page->ref; }
static inline pte_t pte_create(uintptr_t ppn, int type)
{ return (ppn << PTE_PPN_SHIFT) | PTE_V | type; }

void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{ asm volatile("sfence.vma %0" : : "r"(la)); }

#define E_NO_MEM 1
```

**判据中的真实断言（`tools/test_lab4.c`）**

```c
assert(page_insert(boot_pgdir_va, p1, 0, 0) == 0);
assert((pte = get_pte(boot_pgdir_va, 0, 0)) != NULL);
assert((*pte & PTE_V) && pte2page(*pte) == p1 && page_ref(p1) == 1);

assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
assert(page_ref(p1) == 2 && page_ref(p2) == 0);
assert(pte2page(*pte) == p1 && (*pte & (PTE_U | PTE_W)) == 0);

assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, 0) == 0);
free_before = nr_free_pages();
assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, PTE_W) == 0);
assert(page_ref(p3) == 1 && nr_free_pages() == free_before);

page_remove(boot_pgdir_va, 0);
page_remove(boot_pgdir_va, PGSIZE);
page_remove(boot_pgdir_va, 2 * PGSIZE);
page_remove(boot_pgdir_va, 2 * PGSIZE);
assert(page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);
```

### [GUARANTEE]

**交付清单**

| # | 交付项 | 精确签名 / 形态 | 位置 |
|---|---|---|---|
| 1 | `page_insert` | `int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)` | `kern/mm/pmm.c`，`//LAB4 function1` 空洞 |
| 2 | `page_remove` | `void page_remove(pde_t *pgdir, uintptr_t la)` | 同上 |
| 3 | 注释标注 | `//LAB4 function1 2512163` | `kern/mm/pmm.c` |

**接口契约**

1. `page_insert` 成功返回 `0`；`get_pte(..., 1)` 返回 `NULL` 时必须返回 `-E_NO_MEM`。
2. `page_insert` 不得改变 `la` 以外任何地址的映射、引用计数或空闲页计数。
3. `page_remove` 对未映射地址调用任意多次都必须无副作用，不 `assert`、不 `panic`。
4. 物理页引用计数满足不变式：**`page->ref` 等于指向该页的有效叶子 PTE 数量**（页表页除外）。
5. 任何 PTE 修改后必须调用 `tlb_invalidate(pgdir, la)`。

**禁止修改范围（硬性）**

1. 禁止修改 `get_pte`、`get_page`、`tlb_invalidate`、`page_init`、`pmm_init`、`check_pgdir`、`check_boot_pgdir`。
2. 禁止修改 `kern/mm/pmm.h`、`kern/mm/mmu.h`、`kern/mm/memlayout.h`。
3. 禁止修改 `kern/process/*`、`kern/schedule/*`、`kern/trap/*`（B/C 的地盘）。
4. 禁止新增全局变量、全局函数或新的头文件。
5. 禁止用 `set_page_ref(page, 1)` 重置引用计数，必须用 `page_ref_inc` / `page_ref_dec`。
6. 禁止在 `page_remove` 中调用 `alloc_page` 或创建页表页。

### [SPECIFICATION]

**page_insert(pgdir, page, la, perm)**

- **Pre**：`pgdir` 有效；`page != NULL`；`perm` 只含权限位；`la` 的页内偏移被忽略。
- **Case 1：`la` 原本无有效映射** → `page->ref` +1；PTE 写成 `pte_create(page2ppn(page), PTE_V | perm)`；TLB 失效；返回 0。
- **Case 2：`la` 已映射到同一个 `page`** → `ref` 不变；只更新权限位；不分配、不释放；`nr_free_pages()` 前后相同；返回 0。
- **Case 3：`la` 已映射到另一个 `old_page`** → `old_page->ref` -1，归零则释放；`page->ref` +1；PTE 指向 `page`；返回 0。
- **Case 4：`get_pte(..., 1)` 返回 NULL** → 返回 `-E_NO_MEM`；`la` 处原有映射不变；已分配的中间级页表页允许残留。
- **Post**：`get_pte(..., 0)` 返回的 PTE 有效且 `pte2page(*pte) == page`；`page->ref` = 指向该页的有效叶子 PTE 数；TLB 已对 `la` 失效。

**page_remove(pgdir, la)**

- **Pre**：`pgdir` 有效；`la` 可以未映射。
- **Case 1：无有效映射** → 直接返回，不改任何计数，不 panic。
- **Case 2：有有效映射** → 取 `old_page`；叶子 PTE 置 0；`tlb_invalidate`；`old_page->ref` -1，仅归零时 `free_page`。
- **Post**：PTE 为 0（或页表不存在）；连续调用两次与一次等价（幂等）。

**可观察判据**

1. 同一地址先插 `p3`、再插同一个 `p3`（权限不同）：`page_ref(p3)` 前后都为 1，`nr_free_pages()` 不变。
2. 同一地址先映射 `p2`、再映射 `p1`：`page_ref(p1)` +1，`page_ref(p2)` 归 0，PTE 权限被 `perm` 覆盖。
3. 同一地址连续两次 `page_remove`：第二次不改变任何计数，不 panic。
4. `get_pte(..., 1)` 失败时：原有映射仍有效，返回 `-E_NO_MEM`。

### 迭代记录（A 组）

报告 §4.6 已完整记录四版迭代，摘要：

| 版本 | 做法 | 结果 |
|---|---|---|
| v1 | 只判"有映射/无映射"，有映射就先 `page_remove` | 同页重映射时 `ref` 归零、页被释放、PTE 悬空 → `test_lab4_pmm_basic` 第 5 步失败 |
| v2 | 先 `page_ref_inc`，再判 `pte2page(*ptep) == page`，同页则 `page_ref_dec` 抵消 | 三分支共用主流程，通过全部引用计数断言 |
| v3 | 补失败路径（`-E_NO_MEM`）、`page_remove` 改用 `create=0`、统一 `tlb_invalidate` | 幂等性与失败路径断言通过 |
| v4 | 隔离副本 + 最小桩 + 真实 `test_lab4.c` 跑 QEMU | `check_pgdir()`、`check_boot_pgdir()`、`[Test PMM] PASS`、`check_vmm()` 全过 |

---

## 提示词二：进程创建与初始化（2512436-郭睿彤）

### [PROMPT]

```
在 riscv64-ucore Lab 4 骨架的 kern/process/proc.c 中完成
alloc_proc、proc_init、kernel_thread、do_fork、copy_thread、copy_mm。
将模块二的标记写为 //LAB4 function2 2512436，修改范围限于该标记处。

【任务】
把启动时的内核执行流登记为 0 号内核线程 idleproc，再创建 1 号内核线程 initproc，
使调度器能够找到并首次运行它。

【约束】
- 只填 //LAB4 function2 这一处空洞，不修改 get_pid、hash_proc、find_proc、
  setup_kstack、put_kstack、forkret、init_main 等既有函数；
- 不改 kern/process/proc.h、kern/process/entry.S、kern/process/switch.S；
- 不改 kern/schedule/*、kern/mm/*、kern/trap/*（A/C 的地盘）。
```

### [RELY]

**骨架已提供的工具函数（`kern/process/proc.c` 内，完整可用）**

```c
static int get_pid(void);                                  // 带 next_safe 优化的 PID 分配
static void hash_proc(struct proc_struct *proc);           // 加入 pid 哈希桶
struct proc_struct *find_proc(int pid);                    // 按 pid 查找
static int setup_kstack(struct proc_struct *proc);         // alloc_pages(KSTACKPAGE)
static void put_kstack(struct proc_struct *proc);          // free_pages(..., KSTACKPAGE)
static void forkret(void) { forkrets(current->tf); }       // 新线程第一次被调度后的落点
static int init_main(void *arg);                           // 打印三行判据字符串
```

**`struct proc_struct` 的真实字段（`kern/process/proc.h`）**

```c
struct proc_struct {
    enum proc_state state;        // PROC_UNINIT / SLEEPING / RUNNABLE / ZOMBIE
    int pid;
    int runs;
    uintptr_t kstack;
    volatile bool need_resched;
    struct proc_struct *parent;
    struct mm_struct *mm;
    struct context context;       // {ra, sp, s0..s11}
    struct trapframe *tf;
    uintptr_t pgdir;              // 页表根**物理地址**
    uint32_t flags;
    char name[PROC_NAME_LEN + 1];
    list_entry_t list_link;
    list_entry_t hash_link;
};
```

**`entry.S` 的入口约定（决定性约束）**

```asm
.text
.globl kernel_thread_entry
kernel_thread_entry:
	move a0, s1        # 实参从 s1 搬到 a0
	jalr s0            # 函数地址从 s0 取
	jal do_exit
```

因此 `kernel_thread` 构造的初始 trapframe 必须把**函数放 s0、实参放 s1**，
`epc` 指向 `kernel_thread_entry` —— 这是本模块最容易做错的一处。

**`forkrets` 的入口约定（`kern/trap/trapentry.S`）**

```asm
    .globl forkrets
forkrets:
    move sp, a0        # a0 = trapframe 指针
    j __trapret
```

**相关常量与符号**

```c
#define KSTACKPAGE 2
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)
#define CLONE_VM 0x00000100
extern char bootstack[], bootstacktop[];   // kern/mm/pmm.h
uintptr_t boot_pgdir_pa;                   // kern/mm/pmm.c
#define SSTATUS_SIE  0x00000002
#define SSTATUS_SPIE 0x00000020
#define SSTATUS_SPP  0x00000100
```

**判据中的真实断言（`tools/test_lab4.c`）**

```c
assert(current == idleproc);
assert(idleproc != NULL && idleproc->pid == 0);
assert(idleproc->state == PROC_RUNNABLE);
assert(idleproc->kstack == (uintptr_t)bootstack);
assert(idleproc->parent == NULL && idleproc->mm == NULL);
assert(idleproc->tf == NULL && idleproc->runs == 0);
assert(idleproc->flags == 0 && idleproc->context.ra == 0);

assert(initproc != NULL && initproc->pid == 1);
assert(initproc->state == PROC_RUNNABLE);
assert(initproc->parent == idleproc && initproc->mm == NULL);
assert(initproc->context.ra != 0 && initproc->context.sp != 0);
```

### [GUARANTEE]

- 初始化后 `current == idleproc`；`idleproc` 为 PID 0、`PROC_RUNNABLE`、使用 `bootstack`，
  其 `parent`、`mm`、`tf`、`runs`、`flags`、`context.ra` 全为零。
- `initproc` 为 PID 1、`PROC_RUNNABLE`，父进程是 `idleproc`，能按 PID 查到，
  且 `context.ra` / `context.sp` 有效。
- 进程数耗尽时返回 `-E_NO_FREE_PROC`；资源分配失败时返回错误并释放本次已取得的资源。
- 进程公开给调度器之前，PCB、内核栈、中断帧三者必须都已有效。

### [SPECIFICATION]

**内核线程共享启动页表**：PCB 的 `pgdir` 保存 `boot_pgdir_pa`。
`copy_mm` 在本阶段只是保留接口，直接返回 0，不做地址空间复制。

**初始中断帧遵守 RISC-V 入口寄存器约定**：`s0 = fn`、`s1 = arg`、`epc = kernel_thread_entry`；
`status = (sstatus | SPP | SPIE) & ~SIE` —— 返回到 S 模式、`sret` 后开中断。

**`copy_thread` 的两个返回地址不能混淆**：

| 字段 | 值 | 作用 |
|---|---|---|
| `proc->context.ra` | `forkret` | `switch_to` 的 `ret` 落到这里 |
| `proc->context.sp` | `proc->tf` | 恢复后 `sp` 指向中断帧 |
| `proc->tf->epc` | `kernel_thread_entry` | `sret` 落到这里 |
| `proc->tf->gpr.sp` | `kstack + KSTACKSIZE` | `sret` 之后的栈顶 |

**首次执行链**：

```
switch_to → ret 到 forkret
          → forkret 调 forkrets(current->tf)
          → forkrets: move sp, a0; j __trapret
          → RESTORE_ALL + sret
          → 按 tf->epc 进入 kernel_thread_entry
          → move a0, s1; jalr s0
          → 真正执行 init_main("Hello world!!")
```

**一致性要求**：PID 分配、链表与哈希表登记、`nr_process` 计数、`PROC_RUNNABLE` 状态
必须在同一关中断区间内完成；失败路径要按"后申请先释放"的顺序回滚。

### 迭代记录（B 组）

| 轮次 | 现象 | 原因 | 处理 |
|---|---|---|---|
| 1 | 语法检查通过但无法链接 | `proc_init` / `cpu_idle` 缺失，`init.c:39/44` 直接引用 | 属预期：三模块硬依赖，等 C 的模块三 |
| 2 | 核对 `entry.S` 发现入口不用标准 `a0/a1` | 骨架用 `s0/s1` 传函数与实参 | 初始 trapframe 改填 `gpr.s0/s1`，`a0` 清零 |
| 3 | `copy_thread` 的 `sp` 取值不确定 | `kernel_thread` 传 `stack = 0` | 统一写 `stack == 0 ? kstack + KSTACKSIZE : stack` |

---

## 提示词三：调度与上下文切换（2512130-刘婕）

### [PROMPT]

```
在 riscv64-ucore Lab 4 骨架中完成调度闭环的三个空洞，全部标为
//LAB4 function3 2512130：

  1. kern/process/proc.c 的 //LAB4 function3：cpu_idle() 与 proc_run()
  2. kern/schedule/sched.c 的 //LAB4 function3：schedule()
  3. kern/process/switch.S（整份文件）：switch_to()

【任务】
让 idleproc 能在"无事可做"时空转并在 need_resched 置位后让出 CPU；
让 schedule 在 proc_list 上选出下一个 PROC_RUNNABLE 线程；
让 proc_run 完成 current 更新、satp 切换、上下文切换三件事；
让 switch_to 按 struct context 的字段顺序保存/恢复被调用者保存寄存器。

【约束】
- 只填这三处，不修改 wakeup_proc、proc_init、do_fork、trapentry.S、
  kern/process/proc.h、kern/mm/*、kern/trap/*；
- switch.S 需要用 riscv.h 里的 STORE/LOAD/REGBYTES 宏，文件首行加 #include <riscv.h>；
- 注释用中文说明"为什么"，不要写"第一步做什么"。
```

### [RELY]

**`struct context` 的字段顺序（决定 switch.S 的偏移量）**

```c
struct context {
    uintptr_t ra;    // 0*REGBYTES
    uintptr_t sp;    // 1*REGBYTES
    uintptr_t s0;    // 2
    uintptr_t s1;    // 3
    uintptr_t s2;    // 4
    uintptr_t s3;    // 5
    uintptr_t s4;    // 6
    uintptr_t s5;    // 7
    uintptr_t s6;    // 8
    uintptr_t s7;    // 9
    uintptr_t s8;    // 10
    uintptr_t s9;    // 11
    uintptr_t s10;   // 12
    uintptr_t s11;   // 13
};
```

**`riscv.h` 提供的汇编宏（RV64）**

```c
#define STORE sd
#define LOAD  ld
#define LOG_REGBYTES 3
#define REGBYTES (1 << LOG_REGBYTES)   /* = 8 */
```

**`sync.h` 提供的临界区宏**

```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) { intr_disable(); return 1; }
    return 0;
}
static inline void __intr_restore(bool flag) { if (flag) intr_enable(); }
#define local_intr_save(x)    do { x = __intr_save(); } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

**satp 的写法（与 `pmm.c:171` 的 `enable_paging()` 一致）**

```c
write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
```

`0x8000000000000000` = `MODE_SV39(8) << 60`；`pgdir` 存的是**物理地址**，直接右移 12 位。

**`copy_thread` 给新建线程准备的 context（B 模块产出，本模块的输入）**

```c
proc->context.ra = (uintptr_t)forkret;
proc->context.sp = (uintptr_t)proc->tf;
```

**`idleproc` 的特殊性**

- `proc_init` 里 `idleproc->kstack = (uintptr_t)bootstack`，即复用启动栈；
- `idleproc->context` 全零（`alloc_proc` 里 memset），第一次 `switch_to` 保存时才写入真实值；
- `idleproc` **不在** `proc_list` 里（`proc_init` 只 `nr_process++`，没有 `list_add`），
  因此 `schedule` 永远扫不到它，只能作为"无路可走"时的兜底。

**判据中的真实断言（`tools/test_lab4.c`）**

```c
static void test_lab4_thread(void *arg) {
    int id = (int)(intptr_t)arg;
    for (i = 0; i < 3; i++) {
        if (id == 1) test_thread1_ran = 1; else test_thread2_ran = 1;
        current->need_resched = 1;
        schedule();
    }
    while (1) { current->need_resched = 1; schedule(); }
}

void test_lab4_schedule(void) {
    assert(kernel_thread(test_lab4_thread, (void *)1, 0) > 0);
    assert(kernel_thread(test_lab4_thread, (void *)2, 0) > 0);
    for (i = 0; i < 8; i++) { current->need_resched = 1; schedule(); }
    assert(test_thread1_ran && test_thread2_ran);
    lab4_pass("Schedule");
}
```

### [GUARANTEE]

| # | 交付项 | 精确形态 | 位置 |
|---|---|---|---|
| 1 | `cpu_idle` | `void cpu_idle(void)`，永不返回，`need_resched` 为真时 `schedule()` | `kern/process/proc.c` |
| 2 | `proc_run` | `void proc_run(struct proc_struct *proc)`，更新 `current` + 写 `satp` + `switch_to` | 同上 |
| 3 | `schedule` | `void schedule(void)`，环形扫描 `proc_list` 选 `PROC_RUNNABLE` | `kern/schedule/sched.c` |
| 4 | `switch_to` | `void switch_to(struct context *from, struct context *to)`，14 对 STORE/LOAD + `ret` | `kern/process/switch.S` |
| 5 | 注释标注 | `//LAB4 function3 2512130` 三处齐全 | — |

**不变式**

1. `switch_to` 的偏移量与 `struct context` 字段顺序**严格一致**，`ra` 在 0、`sp` 在 1、`s11` 在 13。
2. `switch_to` 不保存 caller-saved 寄存器（`a0`-`a7`、`t0`-`t6`），由编译器按调用约定处理。
3. `proc_run` 中"改 `current` + 切 `satp` + 切栈"必须在同一关中断区间内。
4. `schedule` 必须先清 `current->need_resched`，再切换。
5. 找不到 `PROC_RUNNABLE` 线程时回落到 `idleproc`，不得 `panic`。

### [SPECIFICATION]

**`cpu_idle()`**

- **Pre**：`current` 有效，`init.c` 已 `clock_init()` + `intr_enable()`。
- **行为**：无限循环；`current->need_resched` 为 0 时空转，为 1 时调 `schedule()`。
- **Post**：永不返回。时钟中断每 10 ms 置一次 `need_resched`，因此空转不会让系统永久失去调度机会。

**`proc_run(proc)`**

- **Pre**：`proc != NULL`，其 PCB、内核栈、`context` 均已有效。
- **Case 1：`proc == current`** → 直接返回。
- **Case 2：`proc != current`** → 关中断；`current = proc`；写 `satp`；`switch_to(&prev->context, &next->context)`；开中断（在本线程被再次调度回来时执行）。
- **Post**：CPU 上的执行流、栈、页表都属于 `proc`。

**`schedule()`**

- **Pre**：`current` 有效；可在中断上下文或线程上下文中调用。
- **行为**：关中断；`current->need_resched = 0`；
  `last = (current == idleproc) ? &proc_list : &current->list_link`；
  从 `last` 的下一个开始环形扫描，遇到第一个 `PROC_RUNNABLE` 即停；
  扫完一圈未找到则 `next = idleproc`；`next->runs++`；`next != current` 时 `proc_run(next)`；开中断。
- **Post**：`current` 是一个 `PROC_RUNNABLE` 线程；被切走的线程的 `need_resched` 已清。

**`switch_to(from, to)`**

- **Pre**：`from`、`to` 指向有效的 `struct context`；调用者已关中断。
- **行为**：按序 `STORE ra/sp/s0..s11 → from`；按序 `LOAD ra/sp/s0..s11 → to`；`ret`。
- **Post**：`sp` 已切到 `to` 的上下文；`ret` 回到 `to` 上次停在 `switch_to` 之后的位置（新建线程则落到 `forkret`）。
- **为什么 `sp` 在 `LOAD` 序列里位置无所谓**：后续所有 `LOAD` 的基址是 `a1`（寄存器），不依赖 `sp`；但 `ra` 必须在 `ret` 之前恢复。

**可观察判据**

1. `[Test Schedule] PASS` —— 两个内核线程都被执行到（`test_thread1_ran && test_thread2_ran`）。
2. `this initproc, pid = 1, name = "init"` —— `initproc` 真的被创建并被首次调度。
3. `To U: "Hello world!!".` —— `kernel_thread` 的 `arg` 经 `s1 → a0` 正确传到 `init_main`。
4. `To U: "en.., Bye, Bye. :)"` —— `init_main` 正常返回。

### 迭代记录（C 组）

| 轮次 | 现象 | 原因 | 处理 |
|---|---|---|---|
| 1 | 只改 `current` 不调 `switch_to`，`schedule` 返回后仍在旧栈 | `current` 只是记账，执行流由 `sp/ra` 决定 | 必须调 `switch_to` |
| 2 | 参考 32 位 ucore，在 `proc_run` 里手写 `sp = proc->kstack + KSTACKSIZE` | 那是 x86 版"手工切栈"的做法；riscv 版由 `switch_to` 从 `context.sp` 恢复 | 删掉手写切栈，只保留 `switch_to` |
| 3 | `switch.S` 漏 `#include <riscv.h>`，`STORE`/`REGBYTES` 未定义 | 骨架的 `switch.S` 是空文件，没有模板可抄 | 加 include，与 `trapentry.S` 一致 |
| 4 | `switch.S` 里把 `sp` 放在第一条 `LOAD` 之后又读 `0*REGBYTES(a1)` | 基址是 `a1` 不是 `sp`，顺序不影响正确性，但与常见写法不一致易误读 | 显式注释说明"`sp` 位置无关，`ra` 必须先恢复" |
| 5 | `schedule` 先 `switch_to` 再清 `need_resched` | 被切走的线程回来时带着旧置位，重复调度 | 改为先清后切 |
