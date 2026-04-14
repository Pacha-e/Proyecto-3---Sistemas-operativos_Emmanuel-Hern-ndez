#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "date.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
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
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
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

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
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

extern uint ticks;
extern struct spinlock tickslock;

uint64
sys_date(void)
{
  struct rtcdate r;
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  // Fecha base fija
  r.year = 2026;
  r.month = 2;
  r.day = 20;

  int seconds = xticks / 10;

  r.hour = 2; //(seconds / 3600) % 24;
  r.minute = 38; //(seconds / 60) % 60;
  r.second = seconds % 60;

  uint64 addr;
  argaddr(0, &addr);

  if(copyout(myproc()->pagetable, addr, (char*)&r, sizeof(r)) < 0)
    return -1;

  return 0;
}

uint64
sys_halt(void)
{
   *(uint32*)0x100000 = 0x5555;

  return 0;
}


uint64
sys_setshm(void)
{
  struct proc *p = myproc();
  p->usar_memoria_compartida = 1;
  return 0;
}

uint64
sys_getshm(void)
{
  struct proc *p = myproc();

  if(p->usar_memoria_compartida)
    return p->shm_va;

  return 0;
}

// ==========================================
// Proyecto 2: Comandos EAFITos
// ==========================================

uint64
sys_hello(void)
{
  printf("hello: greetings from the xv6 kernel!\n");
  return 42;
}

uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}

uint64
sys_dumpvm(void)
{
  struct proc *p = myproc();
  printf("=== Page table for pid %d ===\n", p->pid);
  vmprint(p->pagetable);
  return 0;
}

uint64
sys_map_ro(void)
{
  uint64 va;
  argaddr(0, &va);
  struct proc *p = myproc();

  va = PGROUNDDOWN(va);

  if(ismapped(p->pagetable, va))
    return -1;

  char *mem = kalloc();
  if(mem == 0)
    return -1;
  memset(mem, 0, PGSIZE);

  char *msg = "Hola desde el kernel xv6 - pagina solo lectura";
  memmove(mem, msg, strlen(msg) + 1);

  if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_R | PTE_U) != 0) {
    kfree(mem);
    return -1;
  }
  p->map_ro_va = va;
  return 0;
}
