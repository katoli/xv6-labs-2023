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

#define BCSIZE 13

#define HASHKEY(dev, blockno) (((dev << 27) | blockno) % BCSIZE)

struct {
  // for test
  // struct spinlock lock;
  
  struct buf buf[NBUF];
  

  // hash
  struct buf bucket[BCSIZE];
  struct spinlock bklock[BCSIZE];
} bcache;

static inline void
b_insert(uint key, struct buf *b)
{
  b->next = bcache.bucket[key].next;
  bcache.bucket[key].next = b;
  // printf("key: %d b: %p\n", key, bcache.bucket[key].next);
}

// must hold the bklock[key] before call
static inline struct buf *
b_search(uint key, uint dev, uint blockno)
{
  // if(key == 1) printf("key: %d\n", key);
  struct buf *b;
  for(b = bcache.bucket[key].next; b; b=b->next){
    // if(key == 1)printf("dev: %d blockno: %d evien: %d refcnt: %d\n", b->dev, b->blockno, b->evien, b->refcnt);
    if(b->dev == dev && b->blockno == blockno && !b->evien){
      return b;
    }
  }
  return 0;
}
void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");
  for(int i = 0; i < BCSIZE; i++){
    char *id = "bcache.bucket";
    initlock(&bcache.bklock[i], id);
    bcache.bucket[i].next = 0;
  }

  // give buff to each bucket
  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "bcache");
    b->evien = 1;
    b->lastuse = 0;
    b_insert(i%BCSIZE, b);
  }
}



// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // 是否命中cache
  uint key = HASHKEY(dev, blockno);
  acquire(&bcache.bklock[key]);

  // printf("\nbget: key: %d dev: %d blockno: %d\n", key, dev, blockno);
  b_search(1, 0, 0);

  if((b=b_search(key, dev, blockno))){
    b->refcnt++;
    release(&bcache.bklock[key]);
    acquiresleep(&b->lock);
    return b;
  }
  
  
  release(&bcache.bklock[key]);
  // 第一次获取失败，我们需要进行一次evict的操作,先将key的锁释放
  // 接着从其他的bucket里偷，有可能偷到原先的key
  // 符合evict的: b->refcnt==0 || b->evien
  int evict_key = -1;
  struct buf *evict_last = 0; //要驱逐的节点的上一个
  uint time = 0;
  
  for(int i = 0; i < BCSIZE; i++){
    struct buf *bb;
    int tt = -1;
    acquire(&bcache.bklock[i]);
    for(b=&bcache.bucket[i]; b->next; b=b->next){
      if((b->next->evien || b->next->refcnt == 0) &&
          (tt == -1 || (tt > b->next->lastuse))){
        bb = b;
        tt = b->next->lastuse;
      }
    }
    if((tt != -1)&&(evict_key == -1 || (time > tt))){
      if(evict_key != -1)
        release(&bcache.bklock[evict_key]);
      evict_key = i;
      evict_last = bb;
      time = tt;
    }else{
      release(&bcache.bklock[i]);
    }
  }

  // 拿到页面?
  if(evict_key == -1)
    panic("bget: no buffers");
  
  // 创建新的页面
  struct buf *newbuf = evict_last->next;

  // printf("newbuf: %p\n", newbuf);

  // 是否为当前的bucket
  if(evict_key != key){
    evict_last->next = newbuf->next;
    release(&bcache.bklock[evict_key]);

    // 拿key锁后续要修改
    acquire(&bcache.bklock[key]);
  }

  // 如果evict_key == key 则 key的锁已经拿着了
  // 再次查看是否命中cache
  if((b=b_search(key, dev, blockno))){
    // 如果命中的情况下，新页面是没用的，需要丢弃
    if(evict_key != key){
      newbuf->evien = 1;
      newbuf->lastuse = 0;

      b_insert(key, newbuf);
    }
    b->refcnt++;
    release(&bcache.bklock[key]);
    acquiresleep(&b->lock);
    return b;
  }

  // 如果还是没有命中的话，这个新的页面就拿过来用
  // 这个页面不是key的才需要insert
  if(evict_key != key)
    b_insert(key, newbuf);
  // printf("newbuf: dev: %d blockno: %d recnt: %d\n", newbuf->dev, newbuf->blockno, newbuf->refcnt);
  newbuf->evien = 0;
  newbuf->dev = dev;
  newbuf->blockno = blockno;
  newbuf->valid = 0;
  newbuf->refcnt = 1;
  
  release(&bcache.bklock[key]);
  acquiresleep(&newbuf->lock);
  return newbuf;
  /*
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  */

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  /*
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  */
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
  uint key = HASHKEY(b->dev, b->blockno);
  acquire(&bcache.bklock[key]);
  
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastuse = ticks;
  }
  // printf("\nbrelse: key: %d dev: %d blockno: %d refcnt: %d\n", key, b->dev, b->blockno, b->refcnt);
  release(&bcache.bklock[key]);
}

void
bpin(struct buf *b) {
  uint key = HASHKEY(b->dev, b->blockno);
  acquire(&bcache.bklock[key]);
  b->refcnt++;
  release(&bcache.bklock[key]);
}

void
bunpin(struct buf *b) {
  uint key = HASHKEY(b->dev, b->blockno);
  acquire(&bcache.bklock[key]);
  b->refcnt--;
  release(&bcache.bklock[key]);
}


