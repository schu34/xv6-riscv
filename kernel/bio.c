// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define N_BUCKETS 13

struct bucket{
  struct spinlock lock;
  struct buf* head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[N_BUCKETS];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;



void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache_outer");
  for(int i = 0; i < N_BUCKETS; i++){
    initlock(&bcache.buckets[i].lock, "bcache_inner");
    bcache.buckets[i].head = 0;
  }

  // Create linked list of buffers
  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    struct bucket *buck = &bcache.buckets[i%N_BUCKETS];
    b->next = buck->head;
    buck->head = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  struct bucket *buck = &bcache.buckets[blockno % N_BUCKETS];
  acquire(&buck->lock);
  // Is the block already cached?
  for(b = buck->head; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buck->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = buck->head; b != 0; b = b->next){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&buck->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  //no buffers found in this bucket, lets check the rest till we find one
  for(int i = 0; i < N_BUCKETS; i++){
    if(i == blockno % N_BUCKETS){ continue; }
    struct bucket *bucket_to_try = &bcache.buckets[i];
    acquire(&bucket_to_try->lock);
    struct buf* prev = 0;
    for(b = bucket_to_try->head; b != 0; b = b->next){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        if(!prev){
          bucket_to_try->head = b->next;
        }else {
          prev->next = prev->next->next;
        }
        b->next = buck->head;
        buck->head = b;

        release(&bucket_to_try->lock);
        release(&buck->lock);
        acquiresleep(&b->lock);
        return b;
      }
      prev = b;
    }
    //didn't find a free buffer in this bucket either. release the lock and move on
    release(&bucket_to_try->lock);
  }
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  b->refcnt--;
}

void
bpin(struct buf *b) {
  b->refcnt++;
}

void
bunpin(struct buf *b) {
  b->refcnt--;
}


