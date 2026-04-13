#ifndef CREAR_MEMORIA_COMPARTIDA_H
#define CREAR_MEMORIA_COMPARTIDA_H

#include "proc.h"


extern char *shared_page;
extern int refcount;
extern struct spinlock shm_lock;

int crear_memoria_compartida(struct proc *pA, struct proc *pB);

#endif
