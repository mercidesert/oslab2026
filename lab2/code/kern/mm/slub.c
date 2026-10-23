#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>

//lab2 function4 2512130

/* 尺寸类：2 的幂，覆盖 16 字节到 4096 字节 */
const size_t slub_size_classes[SLUB_NR_CLASS] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};

/* slab 描述符数组。用固定大小的静态数组而不是动态分配，
 * 是为了避免「分配器需要内存来分配内存」的循环依赖。 */
static struct slub_slab slub_slabs[SLUB_MAX_SLABS];
static unsigned int slub_nr_slab = 0;

/* 每个尺寸类一条 slab 链表，元素是 slab 数组的下标 */
static unsigned int slub_class_slab[SLUB_NR_CLASS][SLUB_MAX_SLABS];
static unsigned int slub_class_nr[SLUB_NR_CLASS];

/* 空闲对象借用自己的前 8 字节存放 freelist 指针。
 * 对象处于空闲状态时，调用方不关心其内容，因此可以借用。 */
struct slub_free_obj {
    struct slub_free_obj *next;
};

/* 找到能容纳 size 的尺寸类下标，超出范围返回 -1 */
static int slub_class_of(size_t size) {
    if (size == 0) {
        return -1;
    }
    for (int i = 0; i < SLUB_NR_CLASS; i++) {
        if (size <= slub_size_classes[i]) {
            return i;
        }
    }
    return -1;              /* 超过最大尺寸 */
}

/* 物理页 → 内核虚拟地址。
 * 内核镜像被映射在 KERNBASE 处，物理地址加上 va_pa_offset
 * 即为对应的内核虚拟地址。 */
static inline void *pa_to_va(uintptr_t pa) {
    return (void *)(pa + va_pa_offset);
}

/* 分配一个新的 slab：从 buddy 取一页，切分成若干同尺寸对象 */
static int slub_new_slab(int cls) {
    if (slub_nr_slab >= SLUB_MAX_SLABS) {
        return -1;
    }
    struct Page *p = buddy_alloc_pages(1);
    if (p == NULL) {
        return -1;
    }

    struct slub_slab *s = &slub_slabs[slub_nr_slab];
    s->base = pa_to_va(page2pa(p));
    s->obj_size = (unsigned int)slub_size_classes[cls];
    s->nr_obj = (unsigned int)(PGSIZE / slub_size_classes[cls]);
    s->nr_free = s->nr_obj;
    s->freelist = NULL;

    /* 把页内所有对象串成 freelist */
    for (unsigned int i = 0; i < s->nr_obj; i++) {
        struct slub_free_obj *o =
            (struct slub_free_obj *)((char *)s->base + i * s->obj_size);
        o->next = (struct slub_free_obj *)s->freelist;
        s->freelist = o;
    }

    slub_class_slab[cls][slub_class_nr[cls]++] = slub_nr_slab;
    slub_nr_slab++;
    return 0;
}

/* slub_init - 初始化所有 slab 描述符 */
void slub_init(void) {
    slub_nr_slab = 0;
    for (int i = 0; i < SLUB_NR_CLASS; i++) {
        slub_class_nr[i] = 0;
    }
    for (unsigned int i = 0; i < SLUB_MAX_SLABS; i++) {
        slub_slabs[i].base = NULL;
        slub_slabs[i].freelist = NULL;
        slub_slabs[i].nr_obj = 0;
        slub_slabs[i].nr_free = 0;
        slub_slabs[i].obj_size = 0;
    }
}

/* slub_alloc - 分配一个不小于 size 字节的对象 */
void *slub_alloc(size_t size) {
    int cls = slub_class_of(size);
    if (cls < 0) {
        return NULL;
    }

    /* 先从已有 slab 的 freelist 中取 */
    for (unsigned int i = 0; i < slub_class_nr[cls]; i++) {
        struct slub_slab *s = &slub_slabs[slub_class_slab[cls][i]];
        if (s->freelist != NULL) {
            struct slub_free_obj *o = (struct slub_free_obj *)s->freelist;
            s->freelist = o->next;
            s->nr_free--;
            return (void *)o;
        }
    }

    /* 没有可用的，新建一个 slab */
    if (slub_new_slab(cls) != 0) {
        return NULL;
    }
    struct slub_slab *s = &slub_slabs[slub_class_slab[cls][slub_class_nr[cls] - 1]];
    struct slub_free_obj *o = (struct slub_free_obj *)s->freelist;
    s->freelist = o->next;
    s->nr_free--;
    return (void *)o;
}

/* slub_free - 释放对象
 *
 * 需要由对象的地址反查出它属于哪个 slab 与哪个尺寸类。
 * 这里遍历所有 slab 描述符，找到地址落在其范围内的那一个。
 */
void slub_free(void *obj) {
    if (obj == NULL) {
        return;
    }
    for (unsigned int i = 0; i < slub_nr_slab; i++) {
        struct slub_slab *s = &slub_slabs[i];
        if (s->base == NULL) {
            continue;
        }
        char *p = (char *)obj;
        char *lo = (char *)s->base;
        char *hi = lo + (size_t)s->nr_obj * s->obj_size;
        if (p >= lo && p < hi) {
            /* 确认落在某个对象的边界上 */
            if ((size_t)(p - lo) % s->obj_size != 0) {
                panic("slub_free: address is not object aligned");
            }
            struct slub_free_obj *o = (struct slub_free_obj *)obj;
            o->next = (struct slub_free_obj *)s->freelist;
            s->freelist = o;
            s->nr_free++;
            return;
        }
    }
    panic("slub_free: address does not belong to any slab");
}

/* slub_nr_free_objects - 当前空闲对象总数 */
size_t slub_nr_free_objects(void) {
    size_t total = 0;
    for (unsigned int i = 0; i < slub_nr_slab; i++) {
        if (slub_slabs[i].base != NULL) {
            total += slub_slabs[i].nr_free;
        }
    }
    return total;
}
