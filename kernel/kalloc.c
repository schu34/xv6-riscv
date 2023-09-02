// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void* pa_start, void* pa_end);
void freerange_cpu(void* pa_start, void* pa_end, int cpu_id);
void kfree_cpu(void* pa, int cpu_id);

extern char end[]; // first address after kernel.
// defined by kernel.ld.

struct run {
  struct run* next;
};

struct {
  struct spinlock lock[NCPU];
  struct run* freelist[NCPU];
} kmem;

void
kinit() {
  int mem_size_per_cpu = ((uint64)PHYSTOP - (uint64)end) / NCPU;
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem.lock[i], "kmem");
    void* mem_start = end + (mem_size_per_cpu * i);
    void* mem_end = mem_start + mem_size_per_cpu - 1;
    freerange_cpu(mem_start, mem_end, i);
    // freerange(mem_start, mem_end);
  }


}

void
freerange(void* pa_start, void* pa_end) {
  char* p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
  }
}

void
freerange_cpu(void* pa_start, void* pa_end, int cpu_id) {
  char* p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_cpu(p, cpu_id);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void* pa) {
  struct run* r;
  if ((char*)pa < end) {
    panic("out of range (low)");
  }
  if ((uint64)pa >= PHYSTOP) {
    panic("out of range (high)");
  }

  if (((uint64)pa % PGSIZE) != 0)
    panic("not page size multiple");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu_id = cpuid();

  acquire(&kmem.lock[cpu_id]);
  r->next = kmem.freelist[cpu_id];
  kmem.freelist[cpu_id] = r;
  release(&kmem.lock[cpu_id]);

  pop_off();
}

void
kfree_cpu(void* pa, int cpu_id) {
  struct run* r;
  if ((char*)pa < end) {
    panic("out of range (low)");
  }
  if ((uint64)pa >= PHYSTOP) {
    panic("out of range (high)");
  }

  if (((uint64)pa % PGSIZE) != 0)
    panic("not page size multiple");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  acquire(&kmem.lock[cpu_id]);
  r->next = kmem.freelist[cpu_id];
  kmem.freelist[cpu_id] = r;
  release(&kmem.lock[cpu_id]);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void*
kalloc(void) {
  struct run* r;

  push_off();
  int cpu_id = cpuid();
  acquire(&kmem.lock[cpu_id]);
  r = kmem.freelist[cpu_id];

  if (r)
    kmem.freelist[cpu_id] = r->next;
  else {
    //no more memory availible to this CPU, get some from whoever has some.
    for (int i = 0; i < NCPU; i++) {
      if (i == cpu_id) continue;
      acquire(&kmem.lock[i]);
      r = kmem.freelist[i];
      if (r) {
        //we found some memory we can use, advance the freelist and break out of the loop
        kmem.freelist[i] = r->next;
        release(&kmem.lock[i]);
        break;
      }
      release(&kmem.lock[i]);
    }
  }
  release(&kmem.lock[cpu_id]);
  pop_off();

  if (r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

