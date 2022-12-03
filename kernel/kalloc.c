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


// static const int 
#define MAXREFIDX (PHYSTOP / 4096)

int pagerefcounts[MAXREFIDX];
int is_init = 0;

void
kmutaterefcount(uint64 pageaddr, int change){
  // printf("kmutaterefcount: %p, %d\n", pageaddr, change);
  if(pageaddr % 4096)  panic("kmutaterefcount: address must be page aligned");
  if(change != 1 && change != -1) panic("kmutaterefcount: address must be page aligned");
  
  int idx  = pageaddr / 4096;
  //printf("%d\n\n",pagerefcounts[idx]);
  pagerefcounts[idx] += change;
  //printf("%p: ref count after: %d\n\n",pageaddr,pagerefcounts[idx]);
  if(pagerefcounts[idx] < 0) panic("kmutaterefcount: page can't have less than 0 references");
}

void
ksetrefcount(uint64 pageaddr, int newval){
  // printf("kmutaterefcount: %p, %d\n", pageaddr, change);
  if(pageaddr % 4096)  panic("kmutaterefcount: address must be page aligned");
  // if(change != 1 && change != -1) panic("kmutaterefcount: address must be page aligned");
  
  int idx  = pageaddr / 4096;
  //printf("%p: ref count before: %d\n\n",pageaddr,pagerefcounts[idx]);
  pagerefcounts[idx] = newval;
  if(pagerefcounts[idx] < 0) panic("kmutaterefcount: page can't have less than 0 references");
}

int
kgetrefcount(uint64 pageaddr){
  int idx = pageaddr / 4096;
  return pagerefcounts[idx];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  is_init = 1;
  
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
  if(is_init){
    kmutaterefcount((uint64)pa, -1);
    if(kgetrefcount((uint64)pa) >= 1){
      // printf("not freeing %p, has %d refs\n", pa, kgetrefcount((uint64)pa));
     return;
    }
  }  
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

  ksetrefcount((uint64)r, 1);
  return (void*)r;
}

