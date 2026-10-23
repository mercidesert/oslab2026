#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <stdio.h>
#include <string.h>

//lab2 function3 2512130

/* ===================================================================
 * buddy_pmm.c —— Buddy System 页级分配器
 *
 * 本文件基于 刘婕（2512130）交付的 T3/buddy_pmm(1).c 修改。
 * 算法主干（每阶一条空闲链表、二分拆分、向上合并、块首取低偏移）
 * 完全是原作者的设计，未做改动。
 *
 * -------------------------------------------------------------------
 * 修订清单 —— 李思翰（2512163），2026-10-23
 * -------------------------------------------------------------------
 *
 * [FIX-1] buddy_order_up() 在 n 很大时死循环
 *   现象：原实现为
 *       size_t v = 1; while (v < n) { v <<= 1; order++; }
 *   当 n > 2^63 时 v 左移溢出为 0，条件 `0 < n` 恒成立，
 *   函数永远不返回 —— 内核死循环挂死。
 *   触发路径：buddy_alloc_pages((size_t)-1)（即 n > 2^63）。
 *   注意：n = (size_t)1<<30 **不会**触发（1<<30 < 2^63，左移过程不溢出），
 *         她自带的 buddy_test.c 里那条 (size_t)1<<30 用例在原实现上是安全的；
 *         真正会挂死内核的是 (size_t)-1 这类超大值 —— 新设计的 V9 专门覆盖它。
 *   修改：进入循环前先判断 n 是否超过池的最大块 (1<<BUDDY_MAX_ORDER)，
 *         超过直接返回 BUDDY_MAX_ORDER+1（哨兵值），
 *         使 alloc 路径上原本不可达的 `order > BUDDY_MAX_ORDER` 判断变得可达。
 *
 * [FIX-2] 删去 buddy_init() 中冗余的 2^BUDDY_MAX_ORDER 物理页对齐步骤
 *   现象：原实现向底层申请 2 倍池大小的连续空间（8192 页 = 32MB），
 *         再在其中切出一个物理页号按 4096 对齐的子区间当池，
 *         并把首尾两段分别还给底层。
 *   问题：这段对齐对本实现是多余的 —— 本实现的 buddy 关系是
 *         在**池内页偏移**上做异或（buddy_offset），不是物理页号异或，
 *         池首的物理地址不需要按 4096 对齐；池只需要 4096 个物理连续页。
 *   后果：申请量翻倍（32MB 而不是 16MB）、在全局空闲区里挖出两个洞
 *         再分别归还、底层连续空间不足时 panic 的概率高一个数量级。
 *   修改：直接 alloc_pages(1<<BUDDY_MAX_ORDER) 作为池，不再做对齐与拆分。
 *         底层连续空间需求从 8192 页降到 4096 页。
 *
 *   —— 为什么删掉对齐是安全的（这是本条最容易被人质疑的地方，故展开）——
 *   原实现的注释说"池首不对齐，buddy 地址计算就不成立"，那是**经典物理
 *   buddy** 的说法：它要求块首物理地址按 2^order 对齐，buddy 用物理页号
 *   异或求出。本实现不用物理异或，用的是**池内页偏移**异或，因此不需要：
 *     · 池本身是 alloc_pages() 交回来的 4096 个**物理连续**页；
 *     · 不变量：任何一个块都对应池内一段**连续**的偏移区间
 *       [off, off + 2^order)。拆分 = 把这段一分为二（两半仍然连续），
 *       合并 = 把两个只差第 order 位的同级兄弟并成一段更长的连续区间。
 *       这条不变量从"整池是最高阶的一个块"开始始终成立；
 *     · 池物理连续 ⇒ 连续偏移区间 ⇒ 块内各页物理连续。
 *   这条不变量由新测试 V10 的 "pages of one block must be physically
 *   contiguous" 实测守着（见 kern/mm/buddy_verify.c）。
 *   性质说明：这是"省一半内存 + 降低底层连续空间不足的概率"的**优化**，
 *   不是正确性 bug —— 她的对齐写法本身没错，只是按本实现的关系用不上。
 *
 * [FIX-3] buddy_free_pages() 完全忽略参数 n
 *   现象：原实现只用 n 判 0，块的实际大小取自 base->property。
 *   问题：调用方若按"申请时请求的页数"释放，与块的真实大小不一致，
 *         例如 buddy_alloc_pages(3) 实际交付 4 页（阶为 2），
 *         随后 buddy_free_pages(p, 1) 会把整个 4 页块归还，
 *         而调用方认为自己只还了 1 页 —— 同一批页会被交给两个使用者，
 *         造成静默的重复分配。仅靠注释无法阻止这种误用，必须有检查。
 *   修改：新增 int buddy_free_pages_checked(struct Page *base, size_t n)，
 *         要求 buddy_order_up(n) == base->property（即 n 落在
 *         (2^(k-1), 2^k] 区间内），也就是"当初请求的页数"与
 *         "实际块大小"两种写法都接受，其它取值一律拒绝并原样返回 -1，
 *         且拒绝时不改动任何状态；原有的 void buddy_free_pages()
 *         改为它的包装，非法参数时 panic（与本文件其它三处检查一致）。
 *
 * [FIX-4] 释放管理池后各阶链表的计数没有清零
 *   现象：原 buddy_release_pool() 只清了每页的标志位并把池还给底层，
 *         但 13 条空闲链表里的 nr_free 仍是旧值。
 *   后果：池已释放的情况下调用 buddy_nr_free_pages() 会返回一个
 *         虚高的数字（而且链表节点还指向已经还给底层的页）。
 *   修改：释放时一并 list_init() 全部链表并把 nr_free 清零；
 *         buddy_nr_free_pages() 在池为空时直接返回 0。
 *
 * [FIX-5] 缺少只读的观察接口
 *   问题：测试方无法知道池有多大、各阶还剩几个空闲块，
 *         "释放后各阶分布是否完全复原"这类最强判据无从验证。
 *   修改：新增 size_t buddy_pool_npages(void) 与
 *         size_t buddy_nr_free_blocks(int order)。
 *
 * [FIX-6] 池内外判定与偏移计算改用 uintptr_t 数值运算
 *   现象：原实现用指针比较 (p < base, p >= base + n) 和指针相减
 *         (p - base) 来判断"是否在池内"。
 *   问题：这两者都隐含要求 p 与 base 属于同一个数组对象；
 *         当传入的页来自 pages[] 之外时，属于未定义行为。
 *         数值地址比较没有这个前提。
 *   修改：统一用 (uintptr_t) 做加减比较，除以 sizeof(struct Page)
 *         得到页偏移。同时在 buddy_init() 里把池内各页残留的
 *         property 值清零，避免上一次池的旧阶数与新池混在一起
 *         （虽然判定空闲还要看 PG_buddy_free 标志，但清零更干净）。
 *
 * [FIX-7] 头文件里只声明未定义的 void buddy_self_test(void)
 *   问题：该函数在实现文件中不存在，一旦有人调用就是链接错误。
 *   修改：从 buddy_pmm.h 中删除该声明。
 *         实际使用的测试入口是 buddy_test.h 的 void buddy_test(void)。
 * =================================================================== */

/* 每个阶一条空闲链表。链表节点是空闲块的块首 Page，
 * 该块的阶数记录在 Page 的 property 字段中。 */
static free_area_t buddy_free_area[BUDDY_NR_ORDER];

/* 管理池：pool_base 指向池首，pool_npages 为池的总页数。
 *
 * [FIX-2] 注意：池首的**物理**地址不需要按 2^BUDDY_MAX_ORDER 对齐。
 * 本实现的 buddy 关系建立在"池内页偏移"上（见 buddy_offset），
 * 块在池内的对齐由算法自身保证；池只要求 4096 个物理连续的页。 */
static struct Page *buddy_pool_base = NULL;
static size_t buddy_pool_size = 0;

/* [FIX-6] 池的地址边界（按字节），以及池内页偏移。
 * 用整数地址比较代替指针比较，避免依赖"两个指针同属一个数组"这一前提。 */
static inline uintptr_t buddy_pool_start(void) {
    return (uintptr_t)buddy_pool_base;
}

static inline uintptr_t buddy_pool_end(void) {
    return (uintptr_t)buddy_pool_base + buddy_pool_size * sizeof(struct Page);
}

/* 池内页偏移（相对 pool_base）。用偏移而不是物理地址参与
 * 按位异或，是因为物理地址含 va_pa_offset 高位。 */
static inline size_t buddy_offset(struct Page *p) {
    return (size_t)(((uintptr_t)p - buddy_pool_start()) / sizeof(struct Page));
}

static inline struct Page *buddy_at(size_t off) {
    return buddy_pool_base + off;
}

/* 向上取整到 2 的幂，返回其指数（order）。
 *
 * [FIX-1] 原实现 `size_t v = 1; while (v < n) { v <<= 1; order++; }`
 * 在 n > 2^63 时 v 溢出为 0，循环永不退出。这里先做范围判断，
 * 使本函数对任意 size_t 输入都能在有限步内返回。
 * 返回 BUDDY_MAX_ORDER+1 表示"超出本池能表达的范围"。 */
static int buddy_order_up(size_t n) {
    if (n == 0) {
        return 0;
    }
    if (n > (((size_t)1) << BUDDY_MAX_ORDER)) {
        return BUDDY_MAX_ORDER + 1;
    }
    int order = 0;
    while ((((size_t)1) << order) < n) {
        order++;
    }
    return order;
}

/* 把块首挂入对应阶的空闲链表 */
static void buddy_insert(struct Page *block, unsigned int order) {
    block->property = order;
    SetPageBuddyFree(block);
    ClearPageBuddyUsed(block);
    list_add(&buddy_free_area[order].free_list, &(block->page_link));
    buddy_free_area[order].nr_free++;
}

/* buddy_release_pool - 把管理池整体归还给底层分配器
 *
 * 归还前要清掉 buddy 自己的状态标志，否则底层 free_pages
 * 内部的 assert(!PageProperty(p)) 会失败。
 *
 * [FIX-4] 同时把 13 条空闲链表及其计数一并清零：池已经还给底层，
 * 链表里残留的节点指向的是不再属于本模块的页，计数也是虚的。 */
static void buddy_release_pool(void) {
    if (buddy_pool_base == NULL) {
        return;
    }
    for (size_t i = 0; i < buddy_pool_size; i++) {
        struct Page *p = buddy_at(i);
        ClearPageBuddyFree(p);
        ClearPageBuddyUsed(p);
        ClearPageProperty(p);
        set_page_ref(p, 0);
    }
    /* [FIX-4] 清空各阶链表与计数 */
    for (unsigned int k = 0; k < BUDDY_NR_ORDER; k++) {
        list_init(&buddy_free_area[k].free_list);
        buddy_free_area[k].nr_free = 0;
    }
    free_pages(buddy_pool_base, buddy_pool_size);
    buddy_pool_base = NULL;
    buddy_pool_size = 0;
}

/* buddy_init - 从当前物理内存管理器申请一个连续页池
 *
 * [FIX-2] 原实现申请 2 倍池大小的空间后切出物理对齐的子区间。
 * 本实现的 buddy 关系建立在池内页偏移上，池首的物理地址不需要
 * 对齐到 2^BUDDY_MAX_ORDER，因此那段对齐步骤是多余的，
 * 删掉后：申请量 8192 页 -> 4096 页（32MB -> 16MB），
 * 不再在全局空闲区里挖两个洞再分别归还，
 * 底层连续空间不足导致 panic 的概率下降一个数量级。
 *
 * 池仍然要求 4096 个**物理连续**的页 —— buddy 的一个块
 * 必须由物理连续的页组成。 */
void buddy_init(void) {
    /* 若已初始化过，先把上一个池归还，保证可重复调用 */
    buddy_release_pool();

    size_t need = (size_t)1 << BUDDY_MAX_ORDER;

    struct Page *base = alloc_pages(need);
    if (base == NULL) {
        panic("buddy_init: cannot obtain %lu contiguous pages from the underlying manager",
              (unsigned long)need);
    }

    /* 初始化各阶链表 */
    for (unsigned int k = 0; k < BUDDY_NR_ORDER; k++) {
        list_init(&buddy_free_area[k].free_list);
        buddy_free_area[k].nr_free = 0;
    }

    buddy_pool_base = base;
    buddy_pool_size = need;

    /* [FIX-6] 清掉池内各页残留的 property 值：
     * 若上一次初始化时用过这个物理区域，块内某些页的 property
     * 里还留着旧阶数。虽然判定"是否空闲"还要看 PG_buddy_free 标志，
     * 但清零之后整个池的状态就是干净可预期的。 */
    for (size_t i = 0; i < need; i++) {
        buddy_at(i)->property = 0;
    }

    /* 整池作为最高阶的一个空闲块 */
    buddy_insert(base, BUDDY_MAX_ORDER);

    cprintf("buddy: pool base pa 0x%016lx, %lu pages, MAX_ORDER %d\n",
            page2pa(base), (unsigned long)buddy_pool_size, BUDDY_MAX_ORDER);
}

/* buddy_alloc_pages - 分配 n 个连续物理页
 *
 * n 不是 2 的幂时向上取整到 2^order，实际交付 2^order 页。
 * [FIX-1] n 过大时由 buddy_order_up 返回哨兵值，下面的
 * `order > BUDDY_MAX_ORDER` 判断现在真正可达，函数返回 NULL
 * 而不是死循环。 */
struct Page *buddy_alloc_pages(size_t n) {
    if (n == 0) {
        return NULL;
    }
    if (buddy_pool_base == NULL) {
        return NULL;
    }

    int order = buddy_order_up(n);
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }

    /* 找不小于 order 的最小可用阶 */
    unsigned int found = order;
    while (found <= BUDDY_MAX_ORDER && buddy_free_area[found].nr_free == 0) {
        found++;
    }
    if (found > BUDDY_MAX_ORDER) {
        return NULL;               /* 池已耗尽 */
    }

    /* 摘下该阶的一个块 */
    struct Page *block = le2page(list_next(&buddy_free_area[found].free_list), page_link);
    list_del(&(block->page_link));
    buddy_free_area[found].nr_free--;

    /* 逐级二分拆分：每一级把右半块挂回对应阶的空闲链表。
     * 左半块继续向下拆，直到达到请求阶。 */
    while (found > order) {
        found--;
        struct Page *right = block + ((size_t)1 << found);
        buddy_insert(right, found);
    }

    /* 交付给调用方的块首：记录其阶数，标记为已分配 */
    block->property = order;
    ClearPageBuddyFree(block);
    SetPageBuddyUsed(block);
    return block;
}

/* buddy_free_pages_checked - 释放以 base 为首的连续块（带返回值的版本）
 *
 * [FIX-3] 本函数是原 buddy_free_pages() 的主体，多加了一处
 * "n 与块的阶数是否一致"的校验。
 *
 * 块的阶数由块首记录的 property 决定，实际块大小 = 1 << order。
 * 一次成功的分配，请求的页数 n_req 满足 order_up(n_req) == order，
 * 即 n_req 落在 (2^(order-1), 2^order] 区间内。
 * 因此校验条件取"order_up(n) == order"，
 * 这样"当初请求的页数"和"实际块大小"两种写法都能接受，
 * 而明显对不上的取值（例如把 4 页的块按 1 页释放）会被拒绝。
 *
 * 返回值： 0 = 释放成功；-1 = 拒绝，且不改动任何状态。 */
int buddy_free_pages_checked(struct Page *base, size_t n) {
    if (n == 0) {
        cprintf("buddy: free rejected: n == 0\n");
        return -1;
    }
    if (buddy_pool_base == NULL) {
        cprintf("buddy: free rejected: pool is not initialized\n");
        return -1;
    }
    /* [FIX-6] 用整数地址比较判断是否在池内 */
    if ((uintptr_t)base < buddy_pool_start() || (uintptr_t)base >= buddy_pool_end()) {
        cprintf("buddy: free rejected: address is not inside the buddy pool\n");
        return -1;
    }
    /* base 必须是 buddy 分配出去的块首 */
    if (!PageBuddyUsed(base)) {
        cprintf("buddy: free rejected: address is not a buddy allocated block head\n");
        return -1;
    }

    /* 该块所属的阶由块首记录的 property 决定 */
    unsigned int order = base->property;
    if (order > BUDDY_MAX_ORDER) {
        cprintf("buddy: free rejected: invalid block order %u\n", order);
        return -1;
    }

    /* [FIX-3] n 必须与块的阶数一致 */
    if (buddy_order_up(n) != (int)order) {
        cprintf("buddy: free rejected: n = %lu does not match block order %u (%lu pages)\n",
                (unsigned long)n, order, (unsigned long)(((size_t)1) << order));
        return -1;
    }

    /* 向上合并：只要同阶的 buddy 也空闲，就摘下它并合并。
     * base 自身暂不进入任何链表，等合并完成后再统一登记。 */
    while (order < BUDDY_MAX_ORDER) {
        size_t off = buddy_offset(base);
        size_t buddy_off = off ^ ((size_t)1 << order);

        /* 池边界外没有 buddy */
        if (buddy_off >= buddy_pool_size) {
            break;
        }

        struct Page *buddy = buddy_at(buddy_off);

        /* 判定 buddy 是否空闲：必须同时检查 PG_buddy_free 标志与
         * property 值。只比较 property 会把空闲块内部的页误判
         * 成块首，导致链表被破坏。 */
        if (!PageBuddyFree(buddy) || buddy->property != order) {
            break;
        }

        /* buddy 在同一阶的链表中，摘下它 */
        list_del(&(buddy->page_link));
        buddy_free_area[order].nr_free--;
        ClearPageBuddyFree(buddy);
        buddy->property = 0;

        /* 合并后的块首取页偏移较小者。被并入的那一半
         * 退化为块内普通页，必须清掉其状态标志与阶数，
         * 否则重复释放同一地址时不会被检测出来。 */
        if (buddy_off < off) {
            ClearPageBuddyUsed(base);
            base->property = 0;
            base = buddy;
        }
        ClearPageBuddyUsed(base);
        order++;
    }

    /* 合并完成后，base 是最终块首，统一登记到对应阶的空闲链表。
     * buddy_insert 会把阶数写回 base->property。 */
    buddy_insert(base, order);
    return 0;
}

/* buddy_free_pages - 释放以 base 为首的连续块
 *
 * [FIX-3] 现在 n 会被真正校验，非法参数直接 panic；
 * 需要自行处理非法参数时请用 buddy_free_pages_checked()。
 * 其余行为与原实现一致。 */
void buddy_free_pages(struct Page *base, size_t n) {
    if (buddy_free_pages_checked(base, n) != 0) {
        panic("buddy_free_pages: illegal free request (base = %p, n = %lu)",
              base, (unsigned long)n);
    }
}

/* buddy_nr_free_pages - 池内空闲页总数
 * [FIX-4] 池为空时返回 0，不再返回残留的计数。 */
size_t buddy_nr_free_pages(void) {
    if (buddy_pool_base == NULL) {
        return 0;
    }
    size_t total = 0;
    for (unsigned int k = 0; k < BUDDY_NR_ORDER; k++) {
        total += (size_t)buddy_free_area[k].nr_free << k;
    }
    return total;
}

/* buddy_nr_free_blocks - 某一阶的空闲块个数
 * [FIX-5] 新增的只读观察接口，供测试方验证"释放后各阶分布完全复原"。 */
size_t buddy_nr_free_blocks(int order) {
    if (order < 0 || order >= BUDDY_NR_ORDER) {
        return 0;
    }
    return (size_t)buddy_free_area[order].nr_free;
}

/* buddy_pool_npages - 管理池的总页数
 * [FIX-5] 新增的只读观察接口。 */
size_t buddy_pool_npages(void) {
    return buddy_pool_size;
}

/* buddy_page_offset - 返回页在管理池内的页偏移，池外返回 (size_t)-1
 * [FIX-6] 改用整数地址比较。 */
size_t buddy_page_offset(struct Page *p) {
    if (buddy_pool_base == NULL) {
        return (size_t)-1;
    }
    if ((uintptr_t)p < buddy_pool_start() || (uintptr_t)p >= buddy_pool_end()) {
        return (size_t)-1;
    }
    return buddy_offset(p);
}
