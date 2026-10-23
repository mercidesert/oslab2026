#ifndef __KERN_MM_SLUB_H__
#define  __KERN_MM_SLUB_H__

#include <pmm.h>

/* SLUB —— 任意大小对象的分配器
 *
 * 两层架构：
 *   第一层（页级）  由 buddy 提供整页的分配与归还
 *   第二层（对象级） 把整页切分成固定尺寸的对象，并维护每页
 *                  内部的空闲对象链表
 */

//lab2 function4 2512130

/* 尺寸类：对象大小的档位。最小档位为 16 字节，
 * 因为空闲对象需要借用自己的前 8 字节存 freelist 指针。 */
#define SLUB_NR_CLASS     9
#define SLUB_MAX_SLABS    64
#define SLUB_MAX_OBJECT   4096

/* 一个 slab 描述符：对应从 buddy 拿到的一整页 */
struct slub_slab {
    void *base;              /* 该页的虚拟基址 */
    unsigned int obj_size;   /* 本 slab 中每个对象的字节数 */
    unsigned int nr_obj;     /* 本 slab 切分出的对象总数 */
    unsigned int nr_free;    /* 其中尚未分配的对象数 */
    void *freelist;          /* 空闲对象链表的头 */
};

/* 尺寸类表 */
extern const size_t slub_size_classes[SLUB_NR_CLASS];

/* 初始化：清空所有 slab 描述符 */
void slub_init(void);

/* 分配一个不小于 size 字节的对象，返回其虚拟地址，失败返回 NULL */
void *slub_alloc(size_t size);

/* 释放由 slub_alloc 分配的对象 */
void slub_free(void *obj);

/* 当前空闲对象总数 */
size_t slub_nr_free_objects(void);

/* 自检：覆盖尺寸取整、对象隔离、释放复用、耗尽、边界 */
void slub_self_test(void);

#endif /* ! __KERN_MM_SLUB_H__ */
