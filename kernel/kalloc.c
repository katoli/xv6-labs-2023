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

uint64 refcount[SSIZE];
#define REFCOUNT(va) refcount[(va-PGROUNDUP((uint64)end))/PGSIZE]
struct spinlock pgref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgref, "pgref");
  memset(refcount, 0, sizeof(refcount));
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    kincref((void *)p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&pgref);
  if(--REFCOUNT((uint64)pa) > 0){
    release(&pgref);
    return;
  }

  release(&pgref);
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  if(r)
    REFCOUNT(PGROUNDUP((uint64)r)) = 1;
  
  return (void*)r;
}

void *
knpage(void *pa)
{
  acquire(&pgref);
  uint64 npa;

  if(REFCOUNT(PGROUNDUP((uint64)pa)) <= 1){
    release(&pgref);
    return pa;
  }

  if((npa = (uint64)kalloc()) == 0){
    release(&pgref);
    return 0;
  }
  
  memmove((void *)npa, pa, PGSIZE);
  REFCOUNT(PGROUNDUP((uint64)pa))--;

  release(&pgref);
  return (void *)npa;
}

void
kincref(void *pa)
{
  acquire(&pgref);
  REFCOUNT(PGROUNDUP((uint64)pa))++;
  release(&pgref);
}