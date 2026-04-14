#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "crear_memoria_compartida.h"
#include "spinlock.h"

char *shared_page = 0;
int refcount = 0;
struct spinlock shm_lock;

int
crear_memoria_compartida(struct proc *p, struct proc *np)
{
    acquire(&shm_lock);

    if(shared_page == 0){
        shared_page = kalloc();
        if(shared_page == 0){
            release(&shm_lock);
            return -1;
        }
        memset(shared_page, 0, PGSIZE);
        refcount = 2;  // padre + hijo
    } else {
        refcount++;
    }

    release(&shm_lock);

    uint64 pa = (uint64)shared_page;

    // dirección dinámica (NO hardcode)
    uint64 va = PGROUNDUP(p->sz);

    if(mappages(p->pagetable, va, PGSIZE, pa, PTE_W|PTE_R|PTE_U) < 0)
        return -1;

    if(mappages(np->pagetable, va, PGSIZE, pa, PTE_W|PTE_R|PTE_U) < 0)
        return -1;

    p->sz = va + PGSIZE;
    np->sz = va + PGSIZE;


    //  GUARDAR DIRECCIÓN EN LOS PROCESOS
    p->shm_va = va;
    np->shm_va = va;

    p->usar_memoria_compartida = 1;
    np->usar_memoria_compartida = 1;

    return 0;
}

void
init_shm(void)
{
    initlock(&shm_lock, "shm");
}
