# Lab1：全过程标准提示词与验证记录

## 使用说明

本文件汇总本组在 Lab1 实验中使用的全部提示词，按实验指导书 lab0.5 要求的**四段式框架**组织：

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

| 序号 | 提示词主题 | 主要使用成员 |
|------|-----------|------------|
| 提示词一 | 准备源码编译环境并验证工具链 | 2512436-郭睿彤 |
| 提示词二 | 构建最小内核并解释启动代码 | 2512163-李思翰 |
| 提示词三 | GDB 验证复位到内核的启动链 | 2512436-郭睿彤 |
| 提示词四 | 第一轮——建立非空 BSS 测试条件 | 2512436-郭睿彤 |
| 提示词五 | 第二轮——验证非零数据确实被清除 | 2512436-郭睿彤 |
| 提示词六 | 第三轮——恢复最小代码并回归验证 | 2512436-郭睿彤 |
| 提示词七 | 整理报告、提示词和交付材料 | 2512130-刘婕 |
| 提示词八 | 练习1 入口汇编作用分析 | 2512163-李思翰 |

> 提示词八的记录由 2512163-李思翰 按四段式格式补充。

---

## 提示词一：准备源码编译环境并验证工具链

```text
[PROMPT]
任务：为已有 RISC-V Lab1 项目准备源码编译与运行环境。QEMU 从官方源码获取并编译，不使用 Ubuntu 软件仓库中的 QEMU 成品包。
操作要求：先检查现有工具及工程配置；区分 Windows PowerShell 与 WSL Bash。只修改必要配置；安装编译依赖与安装 QEMU 是不同事项。下载失败时先排查并验证压缩包，不能继续解压编译。
输出要求：给出适用于当前终端的操作、成功判据和失败处理；必须依据实际结果确认安装状态。

[RELY]
已有 WSL Ubuntu；RISC-V 交叉编译器命令 riscv64-unknown-elf-gcc。
课程使用 QEMU 4.1.1，目标为 riscv32-softmmu、riscv64-softmmu。
可信上下文为项目 Makefile、工具版本输出和用户提供的终端截图。

[GUARANTEE]
本任务不新增内核接口；交付可用的交叉编译、QEMU 和 GDB 环境及版本记录。

[SPECIFICATION]
Pre-Condition：已安装 WSL，能够访问目标工作目录；工具安装状态待检查。
Post-Condition：WSL 能调用所需编译器、make、QEMU 和 GDB，且有真实版本输出。
Case 1：下载或 TLS 连接失败时，修复获取途径并验证文件可解压，不把下载失败认作成功。
Case 2：缺少 pkg-config 或库依赖时，补充必要构建依赖后再配置。
Requirements：命令不包含 Shell 提示符的 $；sudo 密码由用户输入；中断后优先增量构建，避免无依据清理已有产物。
```

实际结果：源码编译安装 QEMU 4.1.1；初期遇到 TLS 下载错误、缺少 pkg-config、误在 PowerShell 执行 Linux 命令等问题，随后解决。版本输出及最小内核运行证据已在会话展示。

## 提示词二：构建最小内核并解释启动代码

```text
[PROMPT]
任务：检查 Lab1 已有代码是否满足最小内核要求，完成构建和启动验证，并解释汇编入口与 C 初始化的职责。
操作要求：读取实际 entry.S、init.c、console.c、Makefile、kernel.ld；能满足要求时不无故改代码。只能根据真实输出判断成功。
输出要求：说明启动链、核心接口及验证结果；给出需补充的报告内容。

[RELY]
kern/init/entry.S：kern_entry 设置 sp 后转移到 kern_init。
kern/init/init.c：声明 edata/end，初始化相关内存，打印启动文字，进入无限循环。
tools/kernel.ld：入口 kern_entry，链接基址 0x80200000。
Makefile：生成 bin/kernel 与 bin/ucore.img，提供 qemu、debug、gdb 目标。
已有接口：int kern_init(void)（noreturn）；int cprintf(const char *fmt, ...)。
console.c 的 cons_putc 通过 SBI 输出字符。

[GUARANTEE]
保留 kern_entry、kern_init 及现有输出接口；本阶段不新增功能。
交付构建截图、启动截图，以及 la sp, bootstacktop 和 tail kern_init 的解释。

[SPECIFICATION]
Pre-Condition：工具链可用，工程文件完整，执行环境为 WSL。
Post-Condition：生成 ELF 和原始内核镜像；QEMU 输出 (THU.CST) os is loading ...，随后持续执行无限循环。
Requirements：解释启动栈建立、无返回跳转和 SBI 输出链；无限循环没有命令提示符是预期现象，不据此判断卡死故障。
```

实际结果：编译与输出成功。入口初始化 sp 为 bootstacktop，tail 将控制权交给不返回的 kern_init。用 GDB 暂停后观察到 while (1)。

## 提示词三：GDB 验证复位到内核的启动链

```text
[PROMPT]
任务：完成练习2，用 GDB 验证 QEMU 模拟 RISC-V 从复位到内核第一条指令的关键启动阶段，并形成可复核证据。
操作要求：不修改启动代码；使用两个 WSL 终端分别运行调试版 QEMU 与 GDB。按当前截图给出下一步；处理分页、暂停与退出状态。
输出要求：记录初始 PC、最初指令的作用、固件地址、内核入口和栈变化；区分实测结果与原理解释。

[RELY]
make debug 启动 QEMU GDB 服务并暂停；make gdb 读取 bin/kernel 并连接 localhost:1234。
QEMU 4.1.1；复位地址 0x1000，OpenSBI 地址 0x80000000，内核入口 0x80200000。
符号 kern_entry、bootstacktop、kern_init 由实际 ELF 提供，地址以本次构建为准。

[GUARANTEE]
无须新增或修改函数；交付复位反汇编、跳转目标、入口断点及栈初始化证据，并回答练习问题。

[SPECIFICATION]
Pre-Condition：ELF 与加载的镜像来自同一构建，QEMU 在启动处暂停，GDB 连接成功。
Post-Condition：记录复位处最初指令；验证进入 OpenSBI 并最终命中 kern_entry；验证 sp 被设置为启动栈顶及进入 kern_init。
Case 1：固件缺少调试符号时，允许出现 ??，使用地址和反汇编确认阶段。
Case 2：终端为 PowerShell 时，先进入 WSL；QEMU 退出快捷键必须发给运行 QEMU 的终端。
Requirements：不要求单步遍历全部固件。项目用 QEMU loader 预加载镜像，不能仅凭命中入口声称观察到了 OpenSBI 写入镜像；硬件断点可替代普通入口断点。
```

实际证据：PC=0x1000；五条指令为 auipc、addi、csrr、ld、jr，建立参数并从复位数据中取出目标后跳转。0x1018 存放 0x80000000，单步后到达该地址。入口断点命中 0x80200000；两条栈设置指令执行后 sp=0x80203000；随后进入 kern_init。本次原版构建入口跳转目标为 0x8020000a，不能将其当作所有版本固定地址。

## 提示词四：第一轮——建立非空 BSS 测试条件

```text
[PROMPT]
任务：为启动清零行为建立可观察的非空 BSS 测试条件，仅临时修改 kern/init/init.c。
操作要求：备份原文件；在实际文件中加入未显式初始化的 16 字节 volatile 全局探针及清零后的检查。保留原初始化、启动输出和无限循环；不改公共接口或链接脚本。
输出要求：提供真实改动及构建验证方式；说明这是额外测试，后续恢复最小提交版本。

[RELY]
原版实测 edata=end=0x80203008，清零长度为0，不能证明非空区间被正确清零。
kern_init 中现有 memset(edata, 0, end-edata)；链接脚本覆盖 .bss 和 .sbss。
volatile unsigned char bss_probe[16] 可用于内存观察；实际 ELF 需确认其位于清零区间。

[GUARANTEE]
保留 int kern_init(void) 及 noreturn 声明。
新增临时 bss_probe 和检查，探针全零时输出 [BSS test] PASS，否则输出 FAIL。

[SPECIFICATION]
Pre-Condition：原版构建与启动已经通过；已备份原始 init.c。
Post-Condition：镜像保留16字节探针；初始化后检查其值，输出测试判定及原启动文字。
Requirements：避免编译优化或链接回收使探针消失。单独出现 PASS 仅证明检查时为零，不能证明 memset 曾清除人为写入的非零数据。
```

实际结果：重新编译成功，输出 [BSS test] PASS 和启动文字，第一轮通过。

## 提示词五：第二轮——验证非零数据确实被清除

```text
[PROMPT]
任务：用 GDB 为第一轮探针构造非零初始状态，验证内核清零操作实际改变内存。
操作要求：不改源码；在清零前停下，确认探针及清零范围，再写入非零值。在真实清零函数执行前后观察同一内存区域。
输出要求：给出当前状态适用的调试操作，记录清零前、调用参数、返回后及继续运行结果；若错过清零时机，应重启验证。

[RELY]
第一轮镜像含 bss_probe[16]，已通过编译。
已有 kern_init、memset 符号及 edata/end 边界；地址依据当前 ELF。
GDB 支持读写内存、临时断点和 finish；QEMU 已开启调试服务。

[GUARANTEE]
无新增接口；交付非零到零的因果验证截图和继续启动证据。

[SPECIFICATION]
Pre-Condition：暂停在 memset 执行前，ELF 与镜像一致。
Post-Condition：清零前探针为非零；执行 memset 后同一区域全零；继续执行后报告 PASS 并输出启动文字。
Case 1：赋值没有可见效果时，重新读取内存，不能只凭输入命令判定写入成功。
Case 2：断点未命中时暂停并检查当前位置，不在无限循环处误用 finish。
Requirements：记录实际清零地址、填充值和长度；截图必须含写入后与清零后的观察。
```

实际结果：edata=0x80203008、end=0x80203018，长度16；探针16字节为0xa5。命中 memset，参数为起址0x80203008、填充值0、长度16；finish 后16字节均为0x00。继续执行后输出 PASS 和启动文字。完整证据：`report/images/09-bss-clear-verification.png`。

## 提示词六：第三轮——恢复最小代码并回归验证

```text
[PROMPT]
任务：移除 kern/init/init.c 的临时 BSS 测试代码，恢复符合指导书的最终最小内核，并验证回归。
操作要求：在实际文件中删除探针、测试循环、相关变量和测试输出；保留原有 memset、启动文字输出和无限循环。重新编译运行后才确认最终版本通过。
输出要求：说明恢复范围、实际构建及启动证据；报告区分临时测试版与提交版。

[RELY]
原版 init.c 备份及现有测试版；第二轮已证明非空区间的清零操作有效。
现有 Makefile 构建与 qemu 目标；指导书要求输出启动文字后进入死循环。

[GUARANTEE]
保留 int kern_init(void) 及 noreturn 声明；最终代码无 bss_probe 或测试检查接口。

[SPECIFICATION]
Pre-Condition：临时测试证据已保存，旧调试会话和 QEMU 已退出。
Post-Condition：重新生成内核镜像，启动输出原文字，不再输出 [BSS test]，随后持续执行无限循环。
Requirements：不能把临时探针截图描述成最终代码的直接输出；不声称这次恢复提高了性能。
```

实际结果：最终截图显示重新编译 init.c、链接、生成镜像成功；QEMU 输出原启动文字，无 BSS 测试输出。第三轮通过。

## 提示词七：整理报告、提示词和交付材料

```text
[PROMPT]
任务：依据 Lab1 指导书、报告模板及老师最新提交通知，整理已验证的代码、报告、提示词和截图。
操作要求：读取真实工程与截图；先完成本人成员2的材料，再与组内其他成员合并。规范化提示词与原始对话分开标注；不伪造测试、日志、模型版本或推送结果。
输出要求：交付 Markdown 材料，列出未完成项及所需证据。

[RELY]
练习1解释入口操作；练习2记录复位到内核的调试及初始指令作用。
指导书报告要求涵盖整体逻辑、核心模块、实验知识点与 OS 原理的关系，以及未涉及的重要知识点。
老师通知：每组一个公开 Git 仓库；lab1 分支下 code/ 与 report/；后者包含 report.md、prompt.md、images/。
实际工具目录缺少 grade.sh；指导书未将 make grade 列为本次必做验证。

[GUARANTEE]
本任务无须修改内核接口；交付 report.md、prompt.md 和引用的真实截图。仓库上传状态单独核实。

[SPECIFICATION]
Pre-Condition：构建、启动、GDB及三轮验证证据已取得。
Post-Condition：报告结论逐项对应证据，图片引用可解析；提示词覆盖实际协作并标注事后整理；目录遵循老师最新通知。
Requirements：不执行缺失脚本的 grade 目标后伪称通过；报告注明原版清零长度为0、临时版为16。未实际推送前不能声称已提交。
```

## 后续实际提问补充

以下是此前原始记录之后，与验证迭代相关的实际输入摘录；无文字截图输入按其内容概括，不能视为原话。

|实际输入|处理与结果|
|---|---|
|“五、测试与验证 实现测试”|整理测试章节与已有证据。|
|“make grade 需验证吗”|检查目标和脚本，区分模板建议与本次实际可用验证。|
|“提示词要多优化几轮，就是多执行几次命令，然后得到一个更优的代码和结果，所以要继续做优化”|将重复执行调整为具有明确验证目标的三轮测试。|
|“根据上述三轮继续一步步推进落实”|增加临时探针，准备非零清零与最终回归。|
|“根据实验指导书判断运行的是否符合指导书上的要求”|读取指导书，对照已有编译、输出及启动链证据。|
|“继续进行”“是退出这个吗”|指导退出正确的 QEMU 终端并重新编译测试版。|
|截图：第一轮输出 PASS|确认第一轮实际通过。|
|截图：探针赋值及 memset 前后结果|依据实际内存读数修正操作，并确认第二轮通过。|
|“保留这一张吧”|保存完整清零证据图。|
|截图：第二轮继续后输出 PASS|确认人为写入与清零后仍正常启动。|
|“怎么改”|提供移除测试探针、恢复原版 init.c 的代码。|
|截图：最终重新编译与启动输出|确认第三轮回归通过。|
|“结合上述两个文档整理出将本次实验运行整个过程汇总为标准的提示词”|整理本文件的标准四段式任务提示词。|

完整提交仍需合并环境准备阶段的原始输入及其他成员记录。本文件未声称覆盖本组所有对话，也未声称 Git 仓库推送完成。

## 参考依据

- [提示词工程](http://oslab.mobisys.top/lab2026/_book/lab0.5/2_prompt_engineering.html)：任务导向、清晰边界、可验证要求与系统语义。
- [提示词结构](http://oslab.mobisys.top/lab2026/_book/lab0.5/3_prompt_structure.html)：四段式及前置/后置条件；按可独立验证任务组织。
- [Lab1 练习](http://oslab.mobisys.top/lab2026/_book/lab1/lab1_2_1_exercise.html)。


---

# 附录：早期 M2 原始提示词记录（历史状态）

下文保留记录生成时的状态；最新验证结果以前文和 report.md 为准。

# Lab1 M2：提示词与 AI 协作记录

## 范围与环境

- 实验：Lab1 最小可执行内核。
- 模块：M2，使用 GDB 验证启动流程，并整理测试证据和报告材料。
- AI 工具：Codex 桌面应用。
- 底层模型及版本：提交前依据实际会话设置补填，本记录不推测。
- 实验环境：Windows、WSL Ubuntu、VS Code；QEMU 4.1.1、SiFive RISC-V GCC 10.2.0、SiFive GDB 10.1（依据会话中的版本输出）。
- 项目位置：WSL 中 `/mnt/c/Users/13651/Desktop/lab1`。
- 本文件汇总本次 M2 会话，不包含此前全部环境安装、课程视频分析等对话；组内最终 prompt.md 还需合并其他成员与阶段的记录。

## 实际提示词与迭代过程

下表中的提示词保留用户实际文字。截图是当时随消息附带的输入，不是 AI 生成的调试结果；“反馈与操作”和“验证结果”是对会话的概括。

| 序号 | 实际提示词 | 随附输入 | AI 反馈与后续操作 | 验证结果 |
|---|---|---|---|---|
| 1 | 完成第二部分 | `lab1_分工与计划(1).md` | AI 询问第二部分是 M2 还是知识点章节，并检查项目构建配置。 | 待明确任务范围。 |
| 2 | 成员2的 M2：GDB 验证与报告 | 澄清回复 | 明确目标为启动调试、截图、练习2报告。AI 检查 QEMU 4.1.1 复位源码、项目链接脚本及已有反汇编，生成草稿与操作说明。 | 当时 AI 访问 WSL 返回 `Wsl/E_ACCESSDENIED`，只能进行静态分析，没有声称动态验证通过。 |
| 3 | 对于此次作业，我应该做什么 | 无 | 整理双终端启动调试、跟踪复位到内核、保存截图和撰写报告的任务顺序。 | 明确本人负责 M2，不需要从零编写内核。 |
| 4 | 如何连接make gdb | 无 | 指导在两个 WSL 终端分别运行 `make debug` 与 `make gdb`，连接端口 1234。 | 开始实际调试准备。 |
| 5 | 如何退出原来正在运行的 QEMU | 无 | 给出 QEMU 的 Ctrl+A 后按 x，以及进入监视器后执行 quit 的方法。 | 随后截图表明所操作终端实际是 PowerShell。 |
| 6 | （无文字，发送终端截图） | PowerShell 中输入 x，出现 CommandNotFoundException | AI 根据 `PS` 提示符纠正环境判断：该终端没有正在运行的 QEMU；先执行 wsl，再运行 make debug，另开终端运行 make gdb。 | 修正将快捷键字符当作 Shell 命令输入的问题。 |
| 7 | （无文字，发送 GDB 截图） | GDB 启动后停在分页提示 | 指导在分页提示输入 c 并回车，随后关闭分页、检查 PC 和初始指令。 | 此处 c 是分页选择，不是执行内核的 continue。 |
| 8 | 结果截图 | `info registers pc`、`x/5i $pc` 的输出 | AI 分析复位入口的五条指令，指导读取 0x1018 并单步五次。 | 实测 PC 为 0x1000；指令为 auipc、addi、csrr、ld、jr。 |
| 9 | 结果截图 | 0x1018 的数据、五次 si 和 PC 输出 | 确认进入 OpenSBI；解释没有固件调试符号时显示 ?? 是正常现象。指导设置内核入口硬件断点。 | 0x1018 存放 0x80000000；五次单步后 PC 为 0x80000000。 |
| 10 | 这个结果对吗 | 内核入口断点、PC/SP、反汇编及 bootstacktop 输出 | 确认 kern_entry 命中；说明当前 SP 尚未初始化为内核栈，指导单步两条指令。 | PC 为 0x80200000；原 SP 为 0x8001bd80；bootstacktop 为 0x80203000。 |
| 11 | 两次结果 | 栈设置后的结果与进入 kern_init 的结果 | 确认栈设置和跳转成功，指导检查 edata、end 和区间长度，然后 continue。 | SP 为 0x80203000；PC 先为 0x80200008，再到 0x8020000a 的 kern_init。 |
| 12 | 清零区间结果和启动输出截图 | edata/end/长度输出及 QEMU 启动文字截图 | 解释本次清零区间为空，确认内核输出，整理后续交付事项。 | edata=end=0x80203008，长度为 0；QEMU 输出 `(THU.CST) os is loading ...`。 |
| 13 | 将本次提示词汇总 | 无 | 将本次实际提问、环境问题修正和调试验证结果汇总为本文件。 | 供组内合并归档。 |

## 本次实际执行的主要命令

这部分是会话中已展示输出的调试命令，不是新增的用户提示词。

WSL 终端 A：

```bash
cd /mnt/c/Users/13651/Desktop/lab1
make debug
```

WSL 终端 B：

```bash
cd /mnt/c/Users/13651/Desktop/lab1
make gdb
```

GDB 调试过程：

```gdb
set pagination off
info registers pc
x/5i $pc
x/gx 0x1018
si
si
si
si
si
info registers pc
hbreak *0x80200000
continue
info registers pc sp
x/3i $pc
p/x &bootstacktop
si
si
info registers pc sp
si
info registers pc
x/i $pc
p/x &edata
p/x &end
p/d (long)end - (long)edata
continue
```

## 问题、修正与结论

1. **终端环境混淆**：在 PowerShell 中输入 x 导致命令不存在。根据截图纠正为先进入 WSL，再启动调试。
2. **GDB 分页**：启动信息分页阻挡后续显示。继续分页后执行 `set pagination off`，便于截图和检查。
3. **栈初始化时机**：刚命中 kern_entry 时 SP 仍是固件遗留值；执行 la 对应的两条机器指令后才成为 bootstacktop。
4. **符号标注**：SP 地址旁显示 SBI_CONSOLE_PUTCHAR，不代表栈设置失败；同地址可存在不同符号，验证依据是 SP 数值与 bootstacktop 地址一致。
5. **清零区间为空**：本构建 edata 与 end 相等，memset 的长度为零，不应把它记录成清零了一段非空内存。
6. **镜像加载与监视点**：本项目由 QEMU loader 预加载镜像，不能把此过程写成 OpenSBI 复制镜像；连接 GDB 后的写监视点不会追溯已发生的预加载。本次用入口断点验证开始执行。
7. **评测脚本缺失**：项目没有 tools/grade.sh。本次没有执行成功的 make grade，也不声称该项通过。

实际验证路径为：

```text
0x1000（MROM）
  → 0x80000000（OpenSBI）
  → 0x80200000（kern_entry）
  → SP=0x80203000（启动栈）
  → 0x8020000a（kern_init）
  → 输出内核启动文字
```

本次已通过用户实际操作和截图验证核心启动链。未提供完整 GDB 日志文件，因此不能声称日志已归档；退出调试、最终仓库推送和全组报告总装也不在本文件中声称完成。

## 合并与提交

- 补充实际负责人、学号和模型设置。
- 将本文件合入小组 lab1 分支的 `report/prompt.md`，与其他成员记录合并，不覆盖已有内容。
- 将原始截图保存到 `report/images/`，在 report.md 中引用。
- 汇总范围须覆盖全实验，本文件仅负责 M2。

---

## 提示词八：练习1 启动入口分析

**使用成员**：2512163-李思翰

> 以下为成员一实际使用的提示词记录，按实验指导书 lab0.5 的四段式框架标注。

### 练习1：启动入口分析

### Prompt 1.1 — 分析 entry.S 入口汇编

**使用场景：** 分析 `kern/init/entry.S` 中的两条关键指令

```markdown
[PROMPT]
分析以下 RISC-V 内核入口汇编代码（kern/init/entry.S），解释：
1. `la sp, bootstacktop` 完成了什么操作？为什么要设置栈指针？
2. `tail kern_init` 完成了什么操作？与普通函数调用有什么区别？
3. 结合操作系统内核启动流程，说明这两条指令在启动过程中的作用。

[RELY]
代码文件：kern/init/entry.S（包含 .text 段入口 kern_entry 和 .data 段 bootstack 定义）

[GUARANTEE]
- 解释 la 伪指令的实际汇编展开
- 解释 tail 调用与 jal 调用的区别
- 说明完整的三级启动跳流程

[SPECIFICATION]
输出三部分的详细分析，每部分包含"操作"和"目的"两个维度
```

---

### Prompt 1.2 — 追问：如果没有这些指令会怎样

**使用场景：** 深入理解两条指令的必要性

```markdown
[PROMPT]
如果没有以下两条指令，内核启动会怎样？
1. 没有 `la sp, bootstacktop`
2. 没有 `tail kern_init`

[RELY]
- la sp, bootstacktop：将 bootstacktop 地址加载到 sp 寄存器
- tail kern_init：跳转到 kern_init 函数

[GUARANTEE]
- 分别说明缺少每条指令会导致的后果
- 解释 CPU 接下来会执行什么

[SPECIFICATION]
输出两部分的详细分析，每部分包含具体后果和可能看到的错误现象
```

---

### Prompt 1.3 — 对比 jal 和 tail 调用的区别

**使用场景：** 深入理解 `tail kern_init` 为什么用 tail 而不是 jal

```markdown
[PROMPT]
请解释 RISC-V 中 `jal` 和 `tail`（实际是 jr）两种跳转方式的区别，
为什么内核启动时选择 tail 调用而不是 jal？

[RELY]
- jal：保存返回地址到 ra 寄存器，然后跳转
- tail（jr）：不保存返回地址，直接跳转

[GUARANTEE]
- 说明两种调用的寄存器使用差异
- 解释为什么内核初始化适合用 tail 调用

[SPECIFICATION]
输出对比表格，包含：指令格式、ra 寄存器处理、适用场景
```

---

### Prompt 1.4 — 分析 bootstack 内存布局

**使用场景：** 理解内核栈的建立原理

```markdown
[PROMPT]
分析 bootstack 和 bootstacktop 是如何定义的，栈大小是多少？
它们在链接脚本中处于哪个段？为什么栈要这样放置？

[RELY]
- entry.S 中：.space KSTACKSIZE 定义栈空间，bootstacktop 是栈顶
- kernel.ld 中：.data 段，BASE_ADDRESS = 0x80200000
- memlayout.h 中：KSTACKSIZE = KSTACKPAGE * PGSIZE = 2 * 4096 = 8192

[GUARANTEE]
- 计算栈大小
- 说明 bootstack 和 bootstacktop 的关系
- 解释栈在内存布局中的位置

[SPECIFICATION]
输出包含具体数值和分析的说明，包含栈大小计算过程
```

---

### Prompt 1.5 — 协助撰写实验报告

**使用场景：** 整理练习1的实验报告内容

```markdown
[PROMPT]
我需要完成实验报告中的以下部分：
1. 练习1：理解内核启动中的程序入口操作
2. 实验目的章节
3. 整体逻辑分析章节

请帮我按照模板格式撰写报告内容。

[RELY]
核心结论：
- `la sp, bootstacktop`：设置栈指针，建立内核栈，为C语言代码提供运行环境
- `tail kern_init`：尾调用跳转，不保存返回地址，直接跳转到C入口函数 kern_init()
- 启动流程：MROM → OpenSBI → Kernel(0x80200000) → entry.S → kern_init()

[GUARANTEE]
- 练习1的问题解答（la sp、tail kern_init、启动流程）
- 实验目的（基于指导书改写）
- 整体逻辑分析（主线 + 逐步实现）

[SPECIFICATION]
按实验报告模板格式输出，包含 markdown 代码块和表格
```

---

---

## 全组汇总说明

本组共记录八个阶段的提示词。其中：

- **提示词一至七**由 2512436-郭睿彤 在环境准备、启动链验证、BSS 三轮测试与材料整理阶段使用，完整记录见上文。
- **提示词八**由 2512163-李思翰 在练习1 入口汇编分析阶段使用，共 5 个提示词（分析 entry.S 入口汇编、追问缺失两条指令的后果、对比 jal 与 tail、分析 bootstack 内存布局、协助撰写报告），均按四段式框架标注。

本次实验的特殊之处在于：基础代码已由实验指导书提供，因此提示词的重心不在"生成代码"，而在**理解已有代码、验证运行结果、规范记录过程**。所有提示词都明确要求区分"观察到的证据"与"推断的结论"，且最终报告中的每一处运行数据均可回溯到实际截图。

---

## 关于提示词记录范围的说明

本文件汇总的是**本次实验实际使用过的提示词**，不要求为报告的每一个章节单独配一条提示词。实验报告中的实验目的、整体逻辑分析与总结收获等章节属于对实验过程的归纳与表述，其写作依托于前述各阶段提示词所积累的结论，不需要另立提示词条目。
