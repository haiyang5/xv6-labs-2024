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

#define NBUCK 13
#define HASH(id) (id % NBUCK)

char lock_name[NBUCK][10]; 

extern uint ticks;

struct buf buf[NBUF];

struct {
  struct spinlock lock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache[NBUCK];

struct spinlock bcache_lock;

void get_ticks(struct buf *b) {
  acquire(&tickslock);
  b->ticks = ticks;
  release(&tickslock);
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache_lock, "bcache_all");
  // initlock(&tickslock, "tickslock");

  int i;
  for (i = 0; i < NBUCK; i++) {
    snprintf(lock_name[i], 7, "bcache%d", i);
    initlock(&bcache[i].lock, lock_name[i]);

    // Create linked list of buffers
    bcache[i].head.prev = &bcache[i].head;
    bcache[i].head.next = &bcache[i].head;
  }
  for(b = buf; b < buf + NBUF; b++) {
    b->next = bcache[0].head.next;
    b->prev = &bcache[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache[0].head.next->prev = b;
    bcache[0].head.next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *lru_b;

  uint id = HASH(blockno);
  acquire(&bcache[id].lock);
  
  // Is the block already cached?
  for(b = bcache[id].head.next; b != &bcache[id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;

      acquire(&tickslock);
      b->ticks = ticks;
      release(&tickslock);

      release(&bcache[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  lru_b = 0;
  uint min_ticks = 0xffffffff;
  for(int i = id, cycle = 0; cycle != NBUCK; i = (i + 1) % NBUCK) {
    ++cycle;
    // struct buf *last_b = lru_b;
    if (i != id) {
      if(!holding(&bcache[i].lock))
        acquire(&bcache[i].lock);
      else
        continue;
    }
    // last_id = new_id;
    for(b = bcache[i].head.next; b != &bcache[i].head; b = b->next){
      if(b->refcnt == 0 && b->ticks < min_ticks) {
        lru_b = b;
        min_ticks = b->ticks;
      }
    }
    if (lru_b) {
      if (id != i) {
        lru_b->prev->next = lru_b->next;
        lru_b->next->prev = lru_b->prev;
        release(&bcache[i].lock);
        lru_b->next = bcache[id].head.next;
        lru_b->prev = &bcache[id].head;
        lru_b->next->prev = lru_b;
        lru_b->prev->next = lru_b;
      }
      b = lru_b;
      b->dev = dev;
      b->blockno = blockno;
      acquire(&tickslock);
      b->ticks = ticks;
      release(&tickslock);
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
    // if (lru_b != last_b) {
    //   // printf("min_ticks:%d-%d\n", i, min_ticks);
    //   if (last_id >= 0) {
    //     if (last_id != id) release(&bcache[last_id].lock);
    //     // printf("relase..:%d-%d-%d\n", last_id, new_id, i);
    //   }
    // }
    else {
      if (i != id) release(&bcache[i].lock);
    }
  }


  // for (i = 0; i < NBUCK; i++) {
  //   acquire(&bcache[i].lock);
  //   last_id = new_id;
  //   for(b = bcache[i].head.next; b != &bcache[i].head; b = b->next){
  //     printf("%d - %d - %d\n", b - buf, b->refcnt, b->ticks);
  //   }
  // }
  printf("ticks:%d\n", ticks);
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

  uint id = HASH(b->blockno);

  releasesleep(&b->lock);
   
  acquire(&bcache[id].lock);
  b->refcnt--;
  acquire(&tickslock);
  b->ticks = ticks;
  release(&tickslock);

  release(&bcache[id].lock);
}

void
bpin(struct buf *b) {
  
  uint id = HASH(b->blockno);
  acquire(&bcache[id].lock);
  b->refcnt++;
  release(&bcache[id].lock);
}

void
bunpin(struct buf *b) {
  uint id = HASH(b->blockno);
  acquire(&bcache[id].lock);
  b->refcnt--;
  release(&bcache[id].lock);
}


