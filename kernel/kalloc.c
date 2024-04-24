// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  uint64 st;
} pg_count;

uint64 pg_num, st, ed;
void initcount();

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pg_count.lock, "pg_count");
  initcount();
  freerange((void *)st, (void *)ed);
}

void initcount() {
  pg_count.st = PGROUNDUP((uint64)end);
  ed = PGROUNDUP((uint64)PHYSTOP);
  pg_num = (ed - pg_count.st) / PGSIZE;
  memset((void *)pg_count.st, 1, pg_num * 8);
  st = PGROUNDUP(pg_count.st + pg_num * 8);
}

uint8 get_pg_count(uint64 addr) {
  acquire(&pg_count.lock);
  return *(uint8*)(pg_count.st + (addr - st) / PGSIZE * 8);
}
void set_pg_count(uint64 addr, uint8 c) {
  *(uint8*)(pg_count.st + (addr - st) / PGSIZE * 8) = c;
  release(&pg_count.lock);
}

void quote(uint64 addr) {
  if (addr < st || addr >= ed) {
    printf("quote out range: st:%p, addr:%p, ed:%p", st, addr, ed);
    panic("\n");
  }
  uint8 count = get_pg_count(addr);
  if (count <= 0) {
    panic("quote error\n");
  }
  set_pg_count(addr, count + 1);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  uint8 count = get_pg_count((uint64)pa);
  if (count == 1) {
    struct run *r;

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
      panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
    set_pg_count((uint64)pa, 0);
  }
  else if (count > 1) {
    set_pg_count((uint64)pa, count - 1);
  }
  else {
    panic("\nkfree error: count <= 0\n");
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    uint8* count = (uint8*)(pg_count.st + ((uint64)r - st) / PGSIZE * 8);
    if (*count != 0) {
      panic("\nkalloc error: count != 0\n");
    }
    *count = 1;
  }
  return (void*)r;
}
