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

#define PA2IDX(pa) (((uint64)(pa)-KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[(PHYSTOP - KERNBASE) / PGSIZE];
} pageref;

void
kinit()
{
  initlock(&pageref.lock, "pageref");
  for (int i = 0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++) pageref.count[i] = 1;
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  acquire(&pageref.lock);
  if (pageref.count[PA2IDX(pa)]<=0) panic("kfree: refcount");

  pageref.count[PA2IDX(pa)]--;
  if (pageref.count[PA2IDX(pa)]>0) {
    release(&pageref.lock);
    return;
  }
  release(&pageref.lock);

  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    acquire(&pageref.lock);
    pageref.count[PA2IDX(r)] = 1;
    release(&pageref.lock);
  }
  return (void *)r;
}

void ref_inc(void *pa) {
  acquire(&pageref.lock);
  pageref.count[PA2IDX(pa)]++;
  release(&pageref.lock);
}

int ref_get(void *pa) {
  int cnt;
  acquire(&pageref.lock);
  cnt = pageref.count[PA2IDX(pa)];
  release(&pageref.lock);
  return cnt;
}
