struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  int evien;    // need rehash?
  uint dev;
  uint blockno;
  uint lastuse; // the time that last use
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

