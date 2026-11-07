# Lab3 提示词汇总

本文件汇总 Lab3 全部提示词。三份提示词均按实验指导书规定的四段式结构编写：

| 段 | 职责 |
|---|---|
| `[PROMPT]` | 任务目标、操作边界、输出要求。让 AI 知道**能改哪些文件、必须保留什么、要交付什么** |
| `[RELY]` | 从工程真实代码里摘出的接口、宏、结构体字段。把"AI 以为的接口"换成"工程里真实的接口" |
| `[GUARANTEE]` | 必须存在的符号、常量取值、不得引入的依赖。锁定**接口契约** |
| `[SPECIFICATION]` | 每个函数的 Pre-Condition / Post-Condition / Case。锁定**行为契约**：什么输入下必须产生什么输出 |

Lab3 与 Lab2 在 spec 写法上有一个显著差异：**Lab2 判"算法行为对不对"，Lab3 判"字符串对不对 + 控制流会不会重入"。** 因此 Lab3 的三份提示词都把输出字符串、槽位偏移、`epc` 推进量这类"看起来像实现细节"的东西破例写进了 `[SPECIFICATION]`——它们是判据本身，不是实现选择。

---

## 提示词一：trap 入口与上下文恢复（2512163-李思翰）

### [PROMPT]

```
请修改本仓库真实文件 kern/trap/trapentry.S，只在该文件内补全两处被标记为
"#lab3 function1 YOUR CODE" 的位置：
  (1) .macro RESTORE_ALL  —— 补全宏体
  (2) __alltraps           —— 在 SAVE_ALL 之后补上进入 C 处理函数的指令

【任务】
实现 RISC-V Supervisor 模式下的 trap 上下文恢复与 trap 入口，使得：
  - 被 trap 打断的程序的全部通用寄存器（含 sp）在返回后与陷入前逐位相同；
  - 被保存的 4 个 CSR（sstatus / sepc / stval / scause）在返回时被正确写回；
  - trap 现场（struct trapframe）能够按 RISC-V 调用约定交给 C 语言处理函数；
  - 形成完整的闭环："Trap 进入 -> 上下文保存 -> C 语言处理 -> 上下文恢复 -> Trap 返回"，
    即 trap() 返回后应能进入 __trapret，完成上下文恢复并通过 sret 返回。

【对外接口（实现必须满足，不得改变其语义）】
  - RESTORE_ALL：输入 = 当前 Trap frame（经 sp 指向）；输出 = 恢复后的处理器执行状态；
  - __alltraps ：输出 = 将 Trap 上下文正确传递给 C 语言 Trap 处理函数
    void trap(struct trapframe *tf)，即以 struct trapframe * 作为第 1 个实参。

【操作要求】
- 必须直接修改仓库里的真实文件 kern/trap/trapentry.S，不要新建替代文件、
  不要只给代码片段或伪代码；
- 严格保留仓库中已有的全部代码与注释；
- 不得修改 SAVE_ALL、struct trapframe 和 trap() 的函数签名（三者均属既有代码）；
- RESTORE_ALL 必须与已提供的 SAVE_ALL 保持对应关系，不得自行假设一个与实际
  Trap frame 不一致的数据布局；
- 特别注意 SAVE_ALL 中对 x2(sp) 的处理方式与其他通用寄存器并不完全相同，
  不能简单地把所有寄存器按照完全相同的方式恢复；
- 只在上述两个标记位置补全；RESTORE_ALL 的 .endm、__alltraps 之后的 __trapret
  与 sret 一行保持原样；
- 文件开头的 #include <riscv.h> 保持不变，必须使用其中已定义的 LOAD / STORE /
  REGBYTES 宏，不得自行另立等价的宏或改用裸 sd / ld；
- 注释用中文说明"为什么这样写"，不要写"第一步做了什么"式的流水账；
- 不要新增全局函数或全局标签，除 __alltraps / __trapret 之外不要引入新的符号。

【输出要求】
- 先列出你从 SAVE_ALL 推导出的"槽位 ↔ 寄存器"映射表，并逐条与 SAVE_ALL 对账；
- 给出 RESTORE_ALL 的完整汇编；
- 单独列出"恢复顺序的硬约束"及其依据（每条约束说明它来自哪条架构规则）；
- 给出至少一种"如何证明上下文恢复正确"的验证方案（要有可观察的判据，
  不能只说"跑一下看看不崩"）。
```

### [RELY]

以下为本次任务**可信且最小**的上下文（全部取自仓库真实代码，非记忆）：

**宏定义（`libs/riscv.h`）**

```c
#if __riscv_xlen == 64
# define STORE    sd
# define LOAD     ld
#else
# define STORE    sw
# define LOAD     lw
#endif

#define REGBYTES (1 << LOG_REGBYTES)     /* RV64 下 = 8 */
```

**CSR 相关常量（`libs/riscv.h`）**

```c
#define SSTATUS_SIE         0x00000002
#define SSTATUS_SPIE        0x00000020
#define SSTATUS_SPP         0x00000100
#define MIP_STIP            (1 << IRQ_S_TIMER)
#define IRQ_S_TIMER         5
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT           0x3
```

**trap frame 的 C 侧布局（`kern/trap/trap.h`，原文）**

```c
struct pushregs {
    uintptr_t zero;  uintptr_t ra;    uintptr_t sp;    uintptr_t gp;
    uintptr_t tp;    uintptr_t t0;    uintptr_t t1;    uintptr_t t2;
    uintptr_t s0;    uintptr_t s1;    uintptr_t a0;    uintptr_t a1;
    uintptr_t a2;    uintptr_t a3;    uintptr_t a4;    uintptr_t a5;
    uintptr_t a6;    uintptr_t a7;    uintptr_t s2;    uintptr_t s3;
    uintptr_t s4;    uintptr_t s5;    uintptr_t s6;    uintptr_t s7;
    uintptr_t s8;    uintptr_t s9;    uintptr_t s10;   uintptr_t s11;
    uintptr_t t3;    uintptr_t t4;    uintptr_t t5;    uintptr_t t6;
};

struct trapframe {
    struct pushregs gpr;     /* 槽 0~31 */
    uintptr_t status;        /* 槽 32 */
    uintptr_t epc;           /* 槽 33 */
    uintptr_t badvaddr;      /* 槽 34 */
    uintptr_t cause;         /* 槽 35 */
};
```

**36 槽位布局（由 `SAVE_ALL` 与上表共同确定）**

| 槽 | 内容 | 槽 | 内容 |
|---|---|---|---|
| 0 | `x0`（恒 0，占位保持对齐） | 18 | `x18` = `s2` |
| 1 | `x1` = `ra` | 19 | `x19` = `s3` |
| **2** | **`x2` = 旧 `sp`（经 `sscratch` 中转存入）** | 20 | `x20` = `s4` |
| 3 | `x3` = `gp` | 21 | `x21` = `s5` |
| 4 | `x4` = `tp` | 22 | `x22` = `s6` |
| 5 | `x5` = `t0` | 23 | `x23` = `s7` |
| 6 | `x6` = `t1` | 24 | `x24` = `s8` |
| 7 | `x7` = `t2` | 25 | `x25` = `s9` |
| 8 | `x8` = `s0` | 26 | `x26` = `s10` |
| 9 | `x9` = `s1` | 27 | `x27` = `s11` |
| 10 | `x10` = `a0` | 28 | `x28` = `t3` |
| 11 | `x11` = `a1` | 29 | `x29` = `t4` |
| 12 | `x12` = `a2` | 30 | `x30` = `t5` |
| 13 | `x13` = `a3` | 31 | `x31` = `t6` |
| 14 | `x14` = `a4` | 32 | `sstatus` |
| 15 | `x15` = `a5` | 33 | `sepc` |
| 16 | `x16` = `a6` | 34 | `stval`（旧名 `sbadaddr`，0x143） |
| 17 | `x17` = `a7` | 35 | `scause` |

> 即：槽 i（0 ≤ i ≤ 31）存放的就是通用寄存器 `xi`；槽 32~35 依次为
> `sstatus / sepc / stval / scause`。每槽 8 字节，frame 总长 288 字节。
> `struct pushregs` 的 32 个字段顺序与 `x0~x31` 编号严格一致，
> 因此 `SAVE_ALL` 之后 `sp` 即 `struct trapframe` 的基址。

**可调用的既有接口**

| 符号 | 声明位置 | 说明 |
|---|---|---|
| `void trap(struct trapframe *tf)` | `kern/trap/trap.h` | C 侧陷入处理入口，**签名不可改** |
| `void idt_init(void)` | `kern/trap/trap.h` | 设置 `stvec = &__alltraps`、`sscratch = 0` |
| `bool trap_in_kernel(struct trapframe *tf)` | `kern/trap/trap.h` | 依据 `tf->status & SSTATUS_SPP` 判断陷入来源 |

**`__alltraps` 的外部约束**

- `stvec` 低两位为模式编码位，`__alltraps` 起始地址必须 **4 字节对齐**；
- `stvec` 必须指向 `__alltraps`（`idt_init()` 中已固定，符号名不可改）；
- `sscratch` 在 `idt_init()` 中被置 0，且 `SAVE_ALL` 结尾会再次清零——
  **`sscratch` 的非零判据是递归陷入的标志位**，这是仓库既有约定。

### [GUARANTEE]

**交付清单（缺一不可）**

| # | 交付项 | 精确形态 | 位置 |
|---|---|---|---|
| 1 | `RESTORE_ALL` 宏 | `.macro RESTORE_ALL` … `.endm` | `kern/trap/trapentry.S`，紧邻 `SAVE_ALL` 之后 |
| 2 | `__alltraps` 入口 | `.globl __alltraps` + `.align(2)` + `__alltraps:` | 同上 |
| 3 | `__trapret` 出口 | **既有骨架**，仅在 `RESTORE_ALL` 之后保留 `sret` | 同上 |
| 4 | 注释标注 | 两处 `#lab3 function1 YOUR CODE` 替换为**本人学号**，`lab3` 标记保留 | 同上 |

**禁止修改范围（硬性，越界即视为不合格）**

1. 禁止修改 `.macro SAVE_ALL` 的任何一条指令——本模块的槽位定义以它为准
2. 禁止修改 `struct trapframe` / `struct pushregs`
3. 禁止修改 `trap()` 的签名
4. 禁止修改 `kern/trap/trap.c`（模块二的地盘）
5. 禁止修改 `kern/mm/*`、`kern/init/init.c` 等模块三负责的文件
6. **禁止在 `__trapret` 中额外重设 `sepc`**——`epc` 推进由模块二的 `exception_handler` 完成，本模块只负责把 `tf->epc` 忠实写回
7. 禁止新增除 `__alltraps` / `__trapret` 以外的全局符号；禁止引入新的宏定义

**行为保证（对使用者可观测的契约）**

- G1：`RESTORE_ALL` 执行完毕后，`x0~x31` 的值与 `SAVE_ALL` 所保存的完全一致
- G2：`RESTORE_ALL` 执行完毕后，`sstatus / sepc / stval / scause` 与 `SAVE_ALL` 所保存的完全一致
- G3：`__alltraps` 进入 C 处理函数时，`a0` 等于 trap frame 基址，可被直接当作 `struct trapframe *` 使用
- G4：`call trap` 返回之后，`sp` 的值与进入时相同，且仍指向该 trap frame
- G5：恢复过程不产生任何对被中断程序的可见副作用

### [SPECIFICATION]

> 本节只描述"要满足什么"，不描述"怎么实现"。

#### S.1 `RESTORE_ALL` 的契约

**Pre-Condition**

- P1：调用时 `sp` 指向由 `SAVE_ALL` 生成的、布局符合 `struct trapframe` 的内存块
- P2：该内存块内容未被破坏（是陷入瞬间的现场，或被 C 侧按其语义修改过的现场）
- P3：`sscratch` 的"递归陷入判别"语义在本宏执行期间不被破坏

**Post-Condition**

- Q1：`x1`、`x3`~`x31` 的值等于对应槽位的值
- Q2：`x0` 仍为 0（架构保证，不需也无法显式恢复）
- Q3：`sp` 等于槽 2 的值
- Q4：`sstatus`、`sepc`、`stval`、`scause` 分别等于槽 32、33、34、35
- Q5：宏的全部访存都以 P1 的 frame 基址为基准，且**全部在 `sp` 被改写之前完成**
- Q6：宏不依赖任何"调用者预先设置好的"通用寄存器；使用的临时寄存器其自身在恢复完成后必须等于陷入前的值（不得因中转而丢值）
- Q7：`sscratch` 的"是否为 0"与宏执行前一致

**Cases**

| Case | 场景 | 必须满足 |
|---|---|---|
| A-1 | 通用寄存器恢复 | 32 个槽**逐槽**对应同编号寄存器；不存在"一槽被两处读"或"一寄存器被写两次" |
| A-2 | `x0` | 不产生任何对 `x0` 的写入 |
| A-3 | **`sp` 特例** | `sp` 的写回发生在所有其它访存之后；写回 `sp` 那次取数的地址计算仍用 frame 基址 |
| A-4 | CSR 恢复 | 四个 CSR 全部恢复，值来源于对应槽位；中转过程不丢失任何通用寄存器的原值 |
| A-5 | `sstatus` 写回的副作用 | 随后的 `sret` 能按 `sstatus.SPIE` 重新拉起 `SIE` |
| A-6 | `sepc` 的来源 | 取自槽 33；若 C 侧改过 `tf->epc`，恢复后必须是改后的值 |
| A-7 | `sscratch` | 宏不写 `sscratch` |
| A-8 | 原子性 | 对外表现为一次不可分割的状态切换，不存在"部分已恢复"的可观测中间态 |

#### S.2 `__alltraps` 的契约

**Pre-Condition**

- P4：`stvec` 已指向 `__alltraps`，其起始地址 4 字节对齐
- P5：`SAVE_ALL` 已执行完毕，`sp` 指向本次 trap frame

**Post-Condition**

- Q8：C 处理函数第 1 个参数等于 trap frame 基址，可按 `struct trapframe *` 解释
- Q9：C 函数返回后 `sp` 与调用前相同，除 `ra` 外不额外破坏通用寄存器
- Q10：控制流必然流向 `__trapret`，由后者完成恢复并 `sret`

**Cases**

| Case | 场景 | 必须满足 |
|---|---|---|
| A-9 | 参数传递 | 第 1 个参数按 RISC-V 调用约定放在 `a0` |
| A-10 | 调用后的 `sp` | 调用过程不改变 `sp`，返回后仍可定位 frame |
| A-11 | 对齐 | 起始地址 4 字节对齐 |
| A-12 | 符号可见性 | 全局符号，可被 `idt_init()` 以 `&__alltraps` 取址 |
| A-13 | 单一入口 | 陷入到 C 处理函数只有一条路径，`sp` 语义统一 |

#### S.3 Requirements

1. **R1（对称性）**：`RESTORE_ALL` 与 `SAVE_ALL` 在槽位维度严格一一对应，槽位含义以 `SAVE_ALL` 为唯一权威
2. **R2（sp 特例）**：见 A-3，`sp` 必须在最后一步恢复
3. **R3（CSR 写入方式）**：CSR 不得由内存直接写入
4. **R4（调用约定）**：见 A-9 / A-10
5. **R5（对齐）**：见 A-11
6. **R6（原子性）**：见 A-8
7. **R7（边界保护）**：全部访存限定在 `[frame_base, frame_base + 288)`
8. **R8（不越界）**：见 GUARANTEE 的禁止修改范围
9. **R9（可验证性）**：必须给出可复现的验证手段，判据是可观测的等价性，不得以"未崩溃"作为通过标准

### 迭代记录

| 版本 | 做法 | 失败表现 | 根因 | 修改 |
|---|---|---|---|---|
| v1 | 31 个 `LOAD` 平铺，`sp` 混在其中一起恢复 | 汇编能过，运行后现场全错 | `sp` 一旦写回，其后每次取数都换了错误基址 | `sp` 拆出，作为最后一步 |
| v2 | 试图直接由内存写 CSR | 汇编期报错 | `csrw` 源只能是立即数或通用寄存器 | 引入通用寄存器中转，并论证其原值不丢（自身也在恢复名单内） |
| v3 | 中转寄存器未加说明 | 能跑，但读者无法判断是否丢值 | 缺少"为什么安全"的说明 | 补注释说明中转寄存器随后会从对应槽位恢复 |
| v4 | —— | 编译报 `unknown CSR 'sbadaddr'` | 该别名已被本机 binutils 移除（**仓库自带、未修改的 `SAVE_ALL` 里同样报错**） | 改用现行规范名 `stval`（0x143），两者是同一个 CSR |
| v5 | —— | 自建验证程序报 FAIL | **验证程序自身缺陷**：检查前就用通用寄存器保存待比较值，且打印早于比对 | 重写：比对前置于一切打印；临时寄存器只用 `a0`；待比较值存数据段 |

> 教训：v5 是最有价值的一次迭代——**验证工具的缺陷会伪装成被测代码的缺陷**。
> 判别方法：先确认"同一现象是否也能由被测代码之外的改动解释"。

---

## 提示词二：中断与异常处理（2512436-郭睿彤）

### [PROMPT]

**任务**：修改 `kern/trap/trap.c`，实现 `interrupt_handler(struct trapframe *tf)` 与 `exception_handler(struct trapframe *tf)` 两个函数，并只在 `//lab3 function2 YOUR CODE` 标记位置写入代码。

**操作要求**：直接修改项目真实文件，保留现有框架代码与 `lab3 function2` 注释，将 `YOUR CODE` 替换为学号 2512436。不要修改 `trap_dispatch()`、`trap()`、trapframe 结构或其他文件。

**输出要求**：以工程真实代码为准，使用下面 `[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 提供的信息；实现后先对 `trap.c` 做单文件编译检查。

### [RELY]

工程已提供以下可信接口和常量：

- `struct trapframe` 包含 `cause`、`epc` 等字段；
- `IRQ_S_TIMER` 表示监督态定时器中断；
- `IRQ_S_SOFT` 表示监督态软件中断；
- `CAUSE_ILLEGAL_INSTRUCTION` 表示非法指令异常；
- `CAUSE_BREAKPOINT` 表示断点异常；
- `clock_set_next_event()` 用于设置下一次时钟事件；
- 全局变量 `ticks` 记录时钟中断次数；
- `TICK_NUM` 的值为 100；
- `print_ticks()` 输出规定格式的 tick 信息；
- `print_trapframe(tf)` 输出异常现场；
- 中断原因在 `cause` 最高位带有中断标志，实际中断编号需去掉该位。

测试要求输出中应包含以下固定文本：

```text
100 ticks
Supervisor software interrupt
Unknown interrupt: cause 15
Illegal instruction at
Breakpoint at
```

### [GUARANTEE]

交付结果必须满足：

1. 定义 `void interrupt_handler(struct trapframe *tf)`；
2. 定义 `void exception_handler(struct trapframe *tf)`；
3. 保留函数签名和现有分派流程；
4. 不修改 `trap_dispatch()`、`trap()`、`struct trapframe`、`trapentry.S` 或内存管理代码；
5. 未知中断和异常都有可观测的兜底行为；
6. 处理可恢复异常后，返回地址指向异常指令之后；
7. `trap.c` 可由工程现有编译选项独立编译通过。

### [SPECIFICATION]

#### `interrupt_handler()`

Pre-condition：`tf` 指向由 trap 入口保存的有效 trapframe，`tf->cause` 表示一个中断。

Post-condition：

- 对 `IRQ_S_TIMER`，在返回前设置下一次时钟事件并将 `ticks` 增加 1；当 `ticks` 是 `TICK_NUM` 的整数倍时调用规定的 tick 输出函数；
- 对 `IRQ_S_SOFT`，输出测试要求的监督态软件中断文本；
- 对其他中断，输出去除最高中断标志位后的原因编号，并打印 trapframe；
- 不改变 `tf->epc`。

#### `exception_handler()`

Pre-condition：`tf` 指向有效 trapframe，`tf->cause` 表示一个同步异常。

Post-condition：

- 对 `CAUSE_ILLEGAL_INSTRUCTION`，输出异常地址；根据 `tf->epc` 指向的指令编码最低两位判断指令长度，压缩指令推进 2 字节，普通指令推进 4 字节；
- 对 `CAUSE_BREAKPOINT`，输出断点地址，并按本实验 32 位 `ebreak` 的长度将 `epc` 推进 4 字节；
- 对其他异常，打印 trapframe，不执行未规定的恢复操作；
- 处理非法指令或断点后，不得因 `epc` 未推进而重复触发同一异常。

### 迭代记录

| 轮次 | 触发问题 | 提示词修改 |
|---|---|---|
| 1 | 两个 handler 未定义，`-Werror` 下因隐式函数声明编译失败 | 在 `[PROMPT]` 中明确"只在 `//lab3 function2` 标记位置实现"，并要求先做单文件编译 |
| 2 | 完整构建被 `best_fit_pmm.c` 的 `free_list` 未声明阻塞 | 在 `[GUARANTEE]` 中补"可由工程现有编译选项**独立**编译通过"，把模块边界与工程底座问题分开 |
| 3 | 需要确认输出字符串与判据逐字一致 | `[RELY]` 中直接列出五条固定文本；`[SPECIFICATION]` 中补"不得因 `epc` 未推进而重复触发同一异常" |
| 4 | 断点分支写死 `+= 4`，隐含"断点一定是 32 位指令"的假设 | `[SPECIFICATION]` 改为"按该指令的实际长度推进"，与非法指令分支共用同一段判据（本实验注入的 `ebreak` 是 32 位，行为不变） |

---

## 提示词三：lab2 代码回填（2512130-刘婕）

### [PROMPT]

**任务**：修改 `kern/mm/pmm.c` 与 `kern/mm/best_fit_pmm.c`，把 lab2 中已验证通过的物理内存初始化与 Best-Fit 分配器回填到 lab3 骨架，使 lab3 工程恢复 lab2 的物理内存管理能力。

**操作要求**：必须在项目中修改实际文件，而不是仅展示代码。保留骨架已有的中断相关代码（`alloc_pages`/`free_pages`/`nr_free_pages` 外层的 `local_intr_save` / `local_intr_restore` 包裹、`idt_init`、`trap_dispatch`、`print_trapframe`），只在标注位置补全，不删除既有逻辑。不得修改 `struct pmm_manager` 接口、`pmm.h` 中的宏与内联函数，以及 `basic_check` / `best_fit_check` 两个自检函数。

**输出要求**：直接进行文件操作。以 lab3 骨架的真实代码为准，不要引入 lab2 以外的功能（例如 buddy / slub），也不要把 lab2 的 `kern_init()` 内容整体搬过来。

### [RELY]

```c
// 宏定义（kern/mm/memlayout.h、libs/defs.h）
PGSIZE, PGSHIFT, PPN(), ROUNDUP(), PHYSICAL_MEMORY_OFFSET, DRAM_BASE, KERNBASE

// 数据结构（kern/mm/memlayout.h）
struct Page          { ref; flags; property; page_link; }
free_area_t          { list_entry_t free_list; unsigned int nr_free; }
struct pmm_manager   { name; init; init_memmap; alloc_pages;
                       free_pages; nr_free_pages; check; }

// 可用函数
get_memory_base() / get_memory_size()          // kern/driver/dtb.h，dtb_init() 已先于 pmm_init() 调用
SetPageReserved() / SetPageProperty() / PageReserved() / PageProperty()
list_init() / list_add() / list_add_before() / list_del() / le2page()
local_intr_save(x) / local_intr_restore(x)     // kern/sync/sync.h
```

骨架中待补的三处（**第三处没有注释提示**）：

| 位置 | 现状 |
|---|---|
| `pmm.c` 的 `page_init()` | 函数体被替换为 `//lab2 请补充你在lab2中的代码` |
| `pmm.c` 的 `init_pmm_manager()` | `pmm_manager = &default_pmm_manager;` |
| `best_fit_pmm.c` | 只剩 `basic_check` / `best_fit_check`，实现与 `best_fit_pmm_manager` 均缺失 |

### [GUARANTEE]

1. `pmm_manager` 在 `init_pmm_manager()` 执行后指向 `&best_fit_pmm_manager`；
2. `best_fit_pmm.c` 中存在**唯一**的 `const struct pmm_manager best_fit_pmm_manager` 定义，六个函数指针与 `.name` 全部挂上；
3. `basic_check` / `best_fit_check` 各只定义一次（不得因整份粘贴而重复）；
4. `best_fit_pmm.c` 内部以裸名字访问 `free_list` 与 `nr_free`（框架自检函数直接引用这两个符号）；
5. 不新增 `buddy_pmm.*` / `slub.*` 等 lab3 骨架未引用的文件；
6. `kern/` + `libs/` 全部源文件在工程编译选项（`-Wall -Werror`）下 0 error、0 warning-as-error。

### [SPECIFICATION]

#### `page_init()`

Pre-condition：`dtb_init()` 已执行，`get_memory_base()` / `get_memory_size()` 返回有效值；`pmm_manager` 已初始化。

Post-condition：

- 从设备树取得内存范围后，若基址或大小为 0，在注册空闲页**之前**终止并报错；
- 描述符数组紧接 `ROUNDUP(end, PGSIZE)` 之后放置，长度覆盖 `[nbase, npage)`；
- `nbase`、`npage`、`pages` 三个全局量的语义与 lab2 一致（`npage` 是物理页号上界，不是页数）；
- `[DRAM_BASE, 空闲区起点)` 的每一页先被标记为 `PG_reserved`，再由 `init_memmap()` 交给当前分配器；
- 若描述符数组放不下，在注册空闲页**之前**终止。

#### `best_fit_pmm.c` 的五个函数

| 函数 | Post-condition |
|---|---|
| `best_fit_init` | `free_list` 为空链表，`nr_free == 0` |
| `best_fit_init_memmap` | 逐页确认 `PageReserved` 后清标志；`base->property = n` 且置 `PG_property`；按物理地址升序插入；`nr_free += n` |
| `best_fit_alloc_pages` | 返回满足 `property >= n` 的**最小**块；找不到返回 `NULL`；剩余部分插回原位以保持有序 |
| `best_fit_free_pages` | 置 `property` 与 `PG_property` 后按地址升序插入；先查前驱、再查后继各合并一次；合并后块首取地址较低者 |
| `best_fit_nr_free_pages` | 返回 `nr_free` |

#### 一致性要求

- 链表**任何时候**都按物理地址升序，拆分后的剩余块必须插在原位置而非表头；
- 同容量块中优先选中地址较低者（由有序性保证，不需要额外比较）；
- 释放后若与前驱合并，基准指针必须更新为合并后的块首，否则后继合并判断会用错基准；
- 以上性质由 `best_fit_check()` 末尾的 `assert(count == 0 && total == 0)` 统一校验：释放 5 页后链表必须回到与初始完全一致的状态。

### 迭代记录

| 轮次 | 触发问题 | 提示词修改 |
|---|---|---|
| 1 | 只回填了 `page_init()` 函数体，`pmm_manager` 仍指向 `default` | `[RELY]` 中显式列出三处待补位置，并注明"第三处没有注释提示" |
| 2 | 只粘回 `best_fit_pmm.c` 的函数实现，链接报 `undefined reference to 'best_fit_pmm_manager'` | `[GUARANTEE]` 第 2 条要求"六个函数指针与 `.name` 全部挂上"；`[SPECIFICATION]` 补充回填边界为"从 `//lab2 function2-2` 到文件末尾" |
| 3 | 整份粘贴导致 `basic_check` / `best_fit_check` 重复定义 | `[GUARANTEE]` 第 3 条明确"各只定义一次" |
| 4 | 骨架把 `alloc_pages` 等函数用 `local_intr_save` 包住了，担心与 lab2 代码冲突 | `[PROMPT]` 增加"保留骨架已有的中断相关代码，只在标注位置补全" |

---

## 附：三份提示词共同的取舍

Lab3 的判据性质决定了三份提示词都不约而同地做了同一件事：**把"不能错"的细节写死，把"可以自由选择"的部分留空**。

写死的部分包括：五条判据字符串、36 个槽位与 `struct trapframe` 字段的对应关系、`epc` 前进 2 字节还是 4 字节的判断依据、`free_list` / `nr_free` 必须以裸名字存在、`best_fit_pmm_manager` 必须存在。

留空的部分包括：handler 内部用 `switch` 还是 `if-else`、拆分的代码怎么写、用不用辅助函数、注释风格。

这个取舍的直接依据是：**前者的任何一处偏差都会以"判据不通过"的形式立刻暴露，而后者的不同写法在判据上是等价的。** 把后者也写死，等于替 AI 代写代码，反而失去了"用提示词驱动实现"的训练目的。
