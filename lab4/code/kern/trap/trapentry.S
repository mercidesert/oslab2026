#include <riscv.h>

    .macro SAVE_ALL
    .align 2

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save gprs
    STORE  x1,1*REGBYTES(sp)
    STORE  x3,3*REGBYTES(sp)
    STORE  x4,4*REGBYTES(sp)
    STORE  x5,5*REGBYTES(sp)
    STORE  x6,6*REGBYTES(sp)
    STORE  x7,7*REGBYTES(sp)
    STORE  x8,8*REGBYTES(sp)
    STORE  x9,9*REGBYTES(sp)
    STORE  x10,10*REGBYTES(sp)
    STORE  x11,11*REGBYTES(sp)
    STORE  x12,12*REGBYTES(sp)
    STORE  x13,13*REGBYTES(sp)
    STORE  x14,14*REGBYTES(sp)
    STORE  x15,15*REGBYTES(sp)
    STORE  x16,16*REGBYTES(sp)
    STORE  x17,17*REGBYTES(sp)
    STORE  x18,18*REGBYTES(sp)
    STORE  x19,19*REGBYTES(sp)
    STORE  x20,20*REGBYTES(sp)
    STORE  x21,21*REGBYTES(sp)
    STORE  x22,22*REGBYTES(sp)
    STORE  x23,23*REGBYTES(sp)
    STORE  x24,24*REGBYTES(sp)
    STORE  x25,25*REGBYTES(sp)
    STORE  x26,26*REGBYTES(sp)
    STORE  x27,27*REGBYTES(sp)
    STORE  x28,28*REGBYTES(sp)
    STORE  x29,29*REGBYTES(sp)
    STORE  x30,30*REGBYTES(sp)
    STORE  x31,31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    csrr s0, sscratch
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL

    #lab3 function1 2512163

    # ---- 1. 恢复 CSR ----
    # CSR 不能直接由内存写入，必须先把槽位 LOAD 到通用寄存器，再 csrw。
    # 这里借用 t0 (x5) 作中转；t0 自身的值随后会从 slot 5 被恢复，
    # 因此中转不会丢失任何信息。
    LOAD  t0, 32*REGBYTES(sp)
    csrw  sstatus, t0
    LOAD  t0, 33*REGBYTES(sp)
    csrw  sepc, t0
    LOAD  t0, 34*REGBYTES(sp)
    csrw  0x143, t0
    LOAD  t0, 35*REGBYTES(sp)
    csrw  scause, t0

    # ---- 2. 恢复通用寄存器 x1, x3..x31 ----
    # 注意 x0 是硬件常量，x2 (sp) 单独处理（见第 3 步）。
    # 槽位与 SAVE_ALL 严格一一对应：slot i 存的就是 xi。
    LOAD  x1, 1*REGBYTES(sp)
    LOAD  x3, 3*REGBYTES(sp)
    LOAD  x4, 4*REGBYTES(sp)
    LOAD  x5, 5*REGBYTES(sp)
    LOAD  x6, 6*REGBYTES(sp)
    LOAD  x7, 7*REGBYTES(sp)
    LOAD  x8, 8*REGBYTES(sp)
    LOAD  x9, 9*REGBYTES(sp)
    LOAD  x10, 10*REGBYTES(sp)
    LOAD  x11, 11*REGBYTES(sp)
    LOAD  x12, 12*REGBYTES(sp)
    LOAD  x13, 13*REGBYTES(sp)
    LOAD  x14, 14*REGBYTES(sp)
    LOAD  x15, 15*REGBYTES(sp)
    LOAD  x16, 16*REGBYTES(sp)
    LOAD  x17, 17*REGBYTES(sp)
    LOAD  x18, 18*REGBYTES(sp)
    LOAD  x19, 19*REGBYTES(sp)
    LOAD  x20, 20*REGBYTES(sp)
    LOAD  x21, 21*REGBYTES(sp)
    LOAD  x22, 22*REGBYTES(sp)
    LOAD  x23, 23*REGBYTES(sp)
    LOAD  x24, 24*REGBYTES(sp)
    LOAD  x25, 25*REGBYTES(sp)
    LOAD  x26, 26*REGBYTES(sp)
    LOAD  x27, 27*REGBYTES(sp)
    LOAD  x28, 28*REGBYTES(sp)
    LOAD  x29, 29*REGBYTES(sp)
    LOAD  x30, 30*REGBYTES(sp)
    LOAD  x31, 31*REGBYTES(sp)

    # ---- 3. 最后恢复栈指针 sp ----
    # sp 在整个恢复过程中都是 trap frame 的基址，每条 LOAD 都依赖它；
    # 若先写 sp，后续取数就会用错基址。
    # 旧 sp 由 SAVE_ALL 经 sscratch 中转后存放在 slot 2，
    # 因此只能作为最后一步写回（ld 的地址计算先于写回，故安全）。
    LOAD  x2, 2*REGBYTES(sp)

    .endm

    .globl __alltraps
__alltraps:
    SAVE_ALL

    #lab3 function1 2512130
    # 按 RISC-V 调用约定，trapframe 基址（当前 sp）作为 trap() 的第 1 个参数。
    # SAVE_ALL 之后 sp 正好指向 struct trapframe 的 slot 0，与 trap.h 的布局一致。
    mv   a0, sp
    call trap
    # trap() 返回后 sp 未变（callee-saved），仍等于 trapframe 基址，可直接进入 __trapret。

    .globl __trapret
__trapret:
    RESTORE_ALL
    # go back from supervisor call
    sret
 
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
