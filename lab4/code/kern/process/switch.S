//LAB4 function3 2512130
#include <riscv.h>

# void switch_to(struct context *from, struct context *to)
#
# 只保存"被调用者保存寄存器"（callee-saved）：ra、sp、s0-s11。
# caller-saved 的那些（a0-a7、t0-t6）在调用 switch_to 之前已经由编译器
# 按调用约定存到栈上或判定为死值，不需要在这里管。
#
# 字段顺序必须与 proc.h 的 struct context 严格一致：
#   ra, sp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11  ->  0..13
#
# 新建线程的 context 由 copy_thread 填好（ra = forkret，sp = tf），
# 因此第一次从它恢复时，ret 会跳到 forkret 而不是 switch_to 的调用点。
    .globl switch_to
switch_to:
    # ---- 保存 from 的上下文 ----
    STORE ra,  0*REGBYTES(a0)
    STORE sp,  1*REGBYTES(a0)
    STORE s0,  2*REGBYTES(a0)
    STORE s1,  3*REGBYTES(a0)
    STORE s2,  4*REGBYTES(a0)
    STORE s3,  5*REGBYTES(a0)
    STORE s4,  6*REGBYTES(a0)
    STORE s5,  7*REGBYTES(a0)
    STORE s6,  8*REGBYTES(a0)
    STORE s7,  9*REGBYTES(a0)
    STORE s8, 10*REGBYTES(a0)
    STORE s9, 11*REGBYTES(a0)
    STORE s10, 12*REGBYTES(a0)
    STORE s11, 13*REGBYTES(a0)

    # ---- 恢复 to 的上下文 ----
    # 注意 sp 是第 1 条被恢复的：此后所有 LOAD 的地址基址仍是 a1（寄存器），
    # 不依赖 sp，所以换栈不会影响后续读取。
    LOAD ra,  0*REGBYTES(a1)
    LOAD sp,  1*REGBYTES(a1)
    LOAD s0,  2*REGBYTES(a1)
    LOAD s1,  3*REGBYTES(a1)
    LOAD s2,  4*REGBYTES(a1)
    LOAD s3,  5*REGBYTES(a1)
    LOAD s4,  6*REGBYTES(a1)
    LOAD s5,  7*REGBYTES(a1)
    LOAD s6,  8*REGBYTES(a1)
    LOAD s7,  9*REGBYTES(a1)
    LOAD s8, 10*REGBYTES(a1)
    LOAD s9, 11*REGBYTES(a1)
    LOAD s10, 12*REGBYTES(a1)
    LOAD s11, 13*REGBYTES(a1)
    ret
