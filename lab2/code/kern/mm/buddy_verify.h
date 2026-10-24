#ifndef __KERN_MM_BUDDY_VERIFY_H__
#define  __KERN_MM_BUDDY_VERIFY_H__

//lab2 function3 2512163 李思翰

/* Buddy 分配器的独立验证层。
 *
 * 与 kern/mm/buddy_test.c 的分工：
 *   buddy_test.c  —— 刘婕(2512130) 写的实现自检，7 个子测试，
 *                    用框架惯例的裸 assert，失败即 panic。
 *   buddy_verify.c —— 本文件，李思翰(2512163) 写的外部验证，
 *                    10 组用例，全部用"计数式检查"，
 *                    任何一条失败都只计数并打印，不会 panic，
 *                    因此不会因为一条用例挂掉整个内核。
 *
 * 入口：void buddy_verify(void);
 */
void buddy_verify(void);

#endif /* ! __KERN_MM_BUDDY_VERIFY_H__ */
