#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  int i;

  int check_len;
  uint64 va;
  uint64 dstva;
  pagetable_t pagetable;
  uint64 check_va;
  uint32 mask;
  pte_t *pte;

  argaddr(0, &va);
  argint(1, &check_len);
  argaddr(2, &dstva);
  pagetable = myproc()->pagetable;
  mask = 0;
  vmprint(pagetable, 0);
  
  for(i = 0; i < MAXSCAN && i < check_len; i++){
    check_va = va + i*PGSIZE;
    if ((pte = walk(pagetable, check_va, 0)) == 0)
      return -1;
    
    if((*pte & PTE_U) == 0 || (*pte & PTE_V) == 0)
      return -1;
    
    if(*pte & PTE_A){
      mask |= 1 << i;
      *pte &= ~PTE_A;
    }
  }

  if(copyout(pagetable, dstva, (char *)&mask, sizeof(mask)) < 0)
    return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
