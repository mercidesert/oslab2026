#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2512163 李思翰

/* page_init - probe the physical memory space, build the page descriptor
 * array and hand the remaining free pages to the current physical memory
 * manager.
 *
 * The physical layout built here (from low to high):
 *   [ OpenSBI ][ kernel image ][ page descriptor array ][ free pages ... ]
 * The first two parts and the descriptor array itself are marked reserved,
 * only the free pages are registered into the memory manager.
 */
void page_init(void) {
    extern char end[];

    /* 1. the virtual/physical mapping offset, va = pa + va_pa_offset */
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    /* 2. the physical memory range reported by the device tree */
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    if (mem_base == 0 || mem_size == 0) {
        panic("page_init: invalid physical memory info from device tree");
    }
    uint64_t mem_end = mem_base + mem_size;
    if (mem_end < mem_base) {
        panic("page_init: physical memory range overflows");
    }

    /* npage counts the physical pages from 0 up to mem_end, while the
     * descriptor array only covers the pages above DRAM_BASE, a page is
     * addressed by its index (ppn - nbase) */
    npage = PPN(mem_end);
    if (npage <= nbase) {
        panic("page_init: physical memory range is too small");
    }
    size_t n_desc = npage - nbase;

    /* 3. the kernel image ends at the `end` symbol (the end of .bss) */
    uint64_t end_va = (uint64_t)end;
    uint64_t end_pa = (uint64_t)PADDR(end_va);

    /* the descriptor array is placed on the first page after the kernel */
    uint64_t start_va = ROUNDUP(end_va, PGSIZE);
    uint64_t start_pa = ROUNDUP(end_pa, PGSIZE);

    /* 4. the space the descriptor array occupies */
    uint64_t array_bytes = ROUNDUP(n_desc * sizeof(struct Page), PGSIZE);
    if (start_pa + array_bytes > mem_end) {
        panic("page_init: no room left for the page descriptor array");
    }
    uint64_t free_start_pa = start_pa + array_bytes;

    /* 5. publish the descriptor array */
    pages = (struct Page *)start_va;

    /* 6. reserve OpenSBI, the kernel image and the descriptor array */
    struct Page *p = pa2page(DRAM_BASE);
    for (size_t i = 0; i < PPN(start_pa) - nbase; i++) {
        SetPageReserved(p + i);
    }
    p = pa2page(start_pa);
    for (size_t i = 0; i < PPN(free_start_pa) - PPN(start_pa); i++) {
        SetPageReserved(p + i);
    }

    /* 7. give the remaining whole pages to the memory manager */
    size_t n_free = npage - PPN(free_start_pa);
    if (n_free > 0) {
        struct Page *base = pa2page(free_start_pa);
        for (size_t i = 0; i < n_free; i++) {
            SetPageReserved(base + i);
        }
        init_memmap(base, n_free);
    } else {
        cprintf("page_init: no free physical page left\n");
    }

    cprintf("physical memory: base = 0x%016lx, size = 0x%016lx, end = 0x%016lx\n",
            (unsigned long)mem_base, (unsigned long)mem_size,
            (unsigned long)mem_end);
    cprintf("page descriptors: va = 0x%016lx, pa = 0x%016lx, %lu pages\n",
            (unsigned long)start_va, (unsigned long)start_pa,
            (unsigned long)(array_bytes / PGSIZE));
    cprintf("free pages: %lu, npage: %lu\n",
            (unsigned long)n_free, (unsigned long)npage);
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

