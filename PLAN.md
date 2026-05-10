# Proyecto 3 — EAFITos Lazy Allocation
## Plan de Implementación v2 — Consenso LLM Council (Claude + Gemini + Kiro)

**Curso:** SI3003 EAFIT · Prof. José Luis Montoya Pareja  
**Deadline:** Martes 12 mayo 2026  
**Repo P3:** https://github.com/Pacha-e/Proyecto-3---Sistemas-operativos_Emmanuel-Hern-ndez.git  
**Base:** P2 EAFITos (syscalls 27-30 implementadas, ver abajo)

---

## Estado Real del P2 (base confirmada)

| Componente | P2 ya tiene | Acción P3 |
|---|---|---|
| `vmfault(pt, va, read)` en vm.c | ✅ kalloc+mappages para lazy pages | Solo llamar desde trap.c |
| `uvmunmap` sin panic | ✅ usa `continue` para PTEs inválidas | Sin cambio |
| `uvmcopy` sin panic | ✅ usa `continue` para PTEs inválidas | Sin cambio |
| `copyout`/`copyin` lazy-aware | ✅ llaman vmfault() si walkaddr==0 | Sin cambio |
| trap.c handler scause 13/15 | ✅ llama vmfault() + setkilled si falla | **Ampliar** con bounds checks + contador |
| `SBRK_LAZY` en sys_sbrk | ✅ 2-arg: sbrk(n, SBRK_LAZY) | **Modificar**: 1-arg, siempre lazy |
| `pagefault_count` en proc | ❌ | **Agregar** (T4) |
| `struct vregion` en proc | ❌ | **Agregar** (T5) |
| `sys_mapzero` syscall | ❌ | **Agregar** como SYS_mapzero = 31 |
| `tpf.c`, `tlazy.c`, `tmmap_sim.c` | ❌ | **Crear** (T1, T4, T5) |

**Syscalls P2:** SYS_hello=27, SYS_trace=28, SYS_dumpvm=29, SYS_map_ro=30  
**P3 usa:** SYS_mapzero=31

---

## Orden de Implementación

```
T2 (sbrk) → T1 (print format + tpf.c) → T3 (bounds check + contador) → T4 (pagefault_count) → T5 (mapzero)
```

T2 primero porque cambia la API base. T3 ya está parcialmente hecho en P2 — solo ampliar. T4 depende de T3. T5 es independiente.

---

## T1 — Page Fault Observer (8 pts)

P2 ya imprime pero con formato diferente. Cambiar el print en trap.c:

### kernel/trap.c — cambiar formato del print
```c
// ANTES (P2):
printf("[trap] pid %d: %s page fault at va=0x%lx, scause=%ld - killing process\n", ...)

// DESPUÉS (P3 spec):
printf("page fault: pid=%d scause=%ld stval=0x%lx\n", p->pid, r_scause(), va);
```

### user/tpf.c (nuevo)
```c
#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  printf("tpf: test 1 — leer 0x0\n");
  volatile int *p = (int*)0x0;
  int x = *p;
  (void)x;
  printf("tpf: no deberia llegar aqui (test 1)\n");
  exit(0);
}
```

> **Nota:** Solo un test por run porque el proceso muere. Usar scripts o QEMU sessions separadas para testear store fault.

**Agregar a Makefile:** `$U/_tpf\`

---

## T2 — Lazy sbrk() (8 pts)

P2 tiene sys_sbrk de 2 args. P3 requiere 1 arg, siempre lazy para n>0.

### kernel/sysproc.c — cambiar sys_sbrk

```c
uint64 sys_sbrk(void) {
  int n;
  argint(0, &n);
  struct proc *p = myproc();
  uint64 addr = p->sz;
  if(n > 0) {
    // LAZY: solo incrementar sz, NO growproc
    if(addr + n < addr || addr + n > TRAPFRAME) return -1;
    p->sz += n;
  } else if(n < 0) {
    if(growproc(n) < 0) return -1;
  }
  return addr;
}
```

### user/ulib.c — actualizar wrappers (OBLIGATORIO)
```c
// sbrk(): llamada estándar (ahora lazy por defecto)
char* sbrk(int n) {
  return sys_sbrk(n);
}

// sbrklazy(): mantener como alias (backward compat — usertests.c lo usa 17 veces)
char* sbrklazy(int n) {
  return sys_sbrk(n);
}
```

### user/user.h — cambiar declaración
```c
// ANTES:
char*  sys_sbrk(int, int);
// DESPUÉS:
char*  sys_sbrk(int);
```

### kernel/vm.h — ya no se necesitan SBRK_LAZY/SBRK_EAGER
```c
// Puede dejarse por compatibilidad o eliminarse
// #define SBRK_EAGER 1
// #define SBRK_LAZY  2
```

---

## T3 — Lazy Allocation Real (8 pts)

P2 ya tiene el handler. Solo agregar bounds checking y contador.

### kernel/trap.c — ampliar handler existente

```c
} else if(r_scause() == 15 || r_scause() == 13) {
  uint64 va = r_stval();
  struct proc *p = myproc();

  // 1. Verificar vregion primero (T5) — ANTES del lazy check
  if(p->vr.active && va >= p->vr.start && va < p->vr.start + (uint64)p->vr.size) {
    char *mem = kalloc();
    if(mem == 0) { setkilled(p); goto fault_done; }
    memset(mem, 'A', PGSIZE);
    if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_U) != 0) {
      kfree(mem); setkilled(p);
    }
    goto fault_done;
  }

  // 2. Lazy sbrk handler — con bounds check (Kiro: agregar va==0 y sp guard)
  if(va == 0 || va >= p->sz || va < p->trapframe->sp) {
    printf("page fault: pid=%d scause=%ld stval=0x%lx\n", p->pid, r_scause(), va);
    setkilled(p);
  } else {
    uint64 result = vmfault(p->pagetable, va, (r_scause() == 13) ? 1 : 0);
    if(result == 0) {
      printf("page fault: pid=%d scause=%ld stval=0x%lx\n", p->pid, r_scause(), va);
      setkilled(p);
    } else {
      p->pagefault_count++;
    }
  }
  fault_done:;
}
```

> **CRÍTICO (Kiro):** vregion ANTES del lazy check. Si mapzero region está por encima de p->sz, el lazy check la rechazaría.

---

## T4 — Control y Visualización Lazy (8 pts)

### kernel/proc.h — agregar campo
```c
// Dentro de struct proc:
int pagefault_count;
```

### kernel/proc.c — inicializar en allocproc()
```c
p->pagefault_count = 0;
```

### kernel/proc.c — limpiar en freeproc()
```c
p->pagefault_count = 0;
```

### user/tlazy.c (nuevo)
```c
#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  int n = 8;
  char *mem = sbrk(n * 4096);
  printf("tlazy: %d paginas reservadas en 0x%p\n", n, mem);

  printf("tlazy: acceso secuencial...\n");
  for(int i = 0; i < n; i++)
    mem[i * 4096] = (char)(i + 1);

  printf("tlazy: acceso aleatorio...\n");
  mem[3 * 4096] = 'X';
  mem[7 * 4096] = 'Y';

  printf("tlazy: OK — %d page faults esperados\n", n);
  exit(0);
}
```

**Agregar a Makefile:** `$U/_tlazy\`

---

## T5 — mmap Simulation (8 pts)

### kernel/proc.h — agregar struct vregion

```c
// Antes de struct proc o al inicio del archivo:
struct vregion {
  uint64 start;
  int    size;
  int    active;
};

// Dentro de struct proc:
struct vregion vr;
```

### kernel/proc.c — inicializar y limpiar

```c
// allocproc():
p->vr.start  = 0;
p->vr.size   = 0;
p->vr.active = 0;

// freeproc() — CRÍTICO (Kiro): limpiar vregion antes de uvmfree()
if(p->vr.active && p->pagetable) {
  // uvmunmap solo páginas ya mapeadas (puede haber lazy no-allocated)
  uint64 va;
  for(va = p->vr.start; va < p->vr.start + (uint64)p->vr.size; va += PGSIZE) {
    pte_t *pte = walk(p->pagetable, va, 0);
    if(pte && (*pte & PTE_V))
      uvmunmap(p->pagetable, va, 1, 1);
  }
  p->vr.active = 0;
}
```

### kernel/syscall.h
```c
#define SYS_mapzero  31
```

### kernel/syscall.c
```c
// En extern[]:
extern uint64 sys_mapzero(void);

// En syscalls[]:
[SYS_mapzero] sys_mapzero,
```

### kernel/usys.pl
```perl
entry("mapzero");
```

### user/user.h
```c
int mapzero(int);
```

### kernel/sysproc.c — sys_mapzero

```c
uint64 sys_mapzero(void) {
  int size;
  argint(0, &size);
  if(size <= 0) return -1;
  struct proc *p = myproc();
  // Kiro: PGROUNDUP para alinear a página
  uint64 aligned_size = PGROUNDUP((uint64)size);
  p->vr.start  = p->sz;
  p->vr.size   = (int)aligned_size;
  p->vr.active = 1;
  p->sz       += aligned_size;
  return (int64)p->vr.start;
}
```

### user/tmmap_sim.c (nuevo)

```c
#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  int size = 3 * 4096;
  char *p = (char*)mapzero(size);
  if((int64)p < 0) {
    printf("tmmap_sim: mapzero fallo\n");
    exit(1);
  }
  printf("tmmap_sim: region en 0x%p, size=%d\n", p, size);

  printf("tmmap_sim: accediendo pagina 0...\n");
  char c = p[0];
  printf("tmmap_sim: p[0]='%c' (esperado 'A')\n", c);

  printf("tmmap_sim: accediendo pagina 2...\n");
  c = p[2 * 4096];
  printf("tmmap_sim: p[8192]='%c' (esperado 'A')\n", c);

  printf("tmmap_sim: OK\n");
  exit(0);
}
```

**Agregar a Makefile:** `$U/_tmmap_sim\`

---

## Riesgos Identificados (Kiro-CLI)

| Riesgo | Componente | Fix |
|---|---|---|
| `user.h` + `ulib.c` no actualizados al cambiar sys_sbrk | T2 | Sync obligatorio — ver T2 arriba |
| vmfault() no tiene guard `va < sp` ni `va == 0` | T3 | Checks en trap.c ANTES de llamar vmfault() |
| vregion handler DESPUÉS del lazy check | T3/T5 | vregion PRIMERO (Kiro: vregion puede ser > p->sz) |
| freeproc no limpia páginas de vregion | T5 | uvmunmap en freeproc() para PTEs válidas |
| `mappages` remap panic en double-fault | T5 | `ismapped()` ya está en vm.c — vmfault() lo usa |
| `sys_mapzero` size no alineado | T5 | PGROUNDUP(size) en sysproc.c |

---

## Timeline 2 Días

### Día 1 (10 mayo — hoy)
| Hora | Tarea |
|------|-------|
| Setup | ✅ Repo P3 inicializado con base P2, pusheado |
| 2-3h | T2: sys_sbrk + user.h + ulib.c sync |
| 1h | T1: cambiar print format + tpf.c |
| 2h | T3: añadir bounds check a handler P2 existente |

### Día 2 (11 mayo)
| Hora | Tarea |
|------|-------|
| 1h | T4: pagefault_count + tlazy.c |
| 3h | T5: struct vregion + sys_mapzero + trap.c handler + freeproc cleanup + tmmap_sim.c |
| 1h | Tests en QEMU |
| 1h | Features avanzadas + calidad código |
| 1h | Git semántico + README + push final |

---

## Features Avanzadas (10 pts)

| Feature | Pts | Tiempo | Descripción |
|---|---|---|---|
| Tests con asserts | 3 | 45min | Agregar exit(1) en tpf/tlazy/tmmap_sim si resultado inesperado |
| `getpfcount()` syscall | 2 | 30min | SYS_getpfcount=#32, expone p->pagefault_count |
| `--help` en user progs | 2 | 15min | Argparse mínimo en cada programa |
| Señales (SIGSEGV-like) | 2 | 2h | Complejo — omitir si tiempo ajustado |
| /proc-like interface | 3 | 2h | Complejo — omitir si tiempo ajustado |

**Recomendado:** Tests(3) + getpfcount(2) + help(2) = 7pts en ~1.5h

---

## Checklist Final

### Setup
- [x] P2 clonado como base de P3
- [x] Remote origin → P3 GitHub repo
- [x] PLAN.md + Proyecto_3.pdf en repo
- [x] Push inicial a GitHub

### Código
- [ ] T2: sys_sbrk 1-arg, lazy para n>0
- [ ] T2: user.h + ulib.c sincronizados
- [ ] T1: print format correcto en trap.c
- [ ] T1: user/tpf.c creado y en Makefile
- [ ] T3: bounds check va==0 / va>=sz / va<sp en trap.c
- [ ] T3: pagefault_count++ en path exitoso
- [ ] T4: campo pagefault_count en struct proc
- [ ] T4: init/clear en allocproc/freeproc
- [ ] T4: user/tlazy.c creado y en Makefile
- [ ] T5: struct vregion en proc.h + proc init/clear
- [ ] T5: freeproc limpia páginas de vregion (anti-freewalk-panic)
- [ ] T5: SYS_mapzero=31 en syscall.h/c/usys.pl/user.h
- [ ] T5: sys_mapzero con PGROUNDUP en sysproc.c
- [ ] T5: vregion handler ANTES de lazy handler en trap.c
- [ ] T5: user/tmmap_sim.c creado y en Makefile

### Tests QEMU
- [ ] `make qemu` compila sin errores
- [ ] `tpf` → imprime "page fault: pid=X scause=Y stval=0xZ" y muere
- [ ] `echo hi` funciona (lazy allocation no rompe userspace básico)
- [ ] `tlazy` corre y escribe en todas las páginas
- [ ] `tmmap_sim` imprime 'A' en páginas de mapzero
- [ ] `usertests` pasa (sin regresiones en P2)

### Calidad
- [ ] Comentarios WHY en trap.c (no QUÉ, solo POR QUÉ)
- [ ] Sin memory leaks (kfree en todos los error paths)
- [ ] ≥5 commits semánticos (feat:, fix:, test:, refactor:)
- [ ] README actualizado con instrucciones make + test

---

## Rúbrica vs Implementación

| Criterio | Pts | Estado |
|---|---|---|
| T1 Page Fault Observer | 8 | A implementar (base en P2) |
| T2 Lazy sbrk intro | 8 | A implementar (base en P2) |
| T3 Lazy Allocation real | 8 | A implementar (vmfault existe) |
| T4 Control lazy | 8 | A implementar |
| T5 mmap simulation | 8 | A implementar |
| Modularidad | 8 | vmfault() ya modular |
| Documentación | 8 | Agregar WHY comments |
| Estilo | 5 | Seguir estilo xv6 (tabs, naming) |
| Memoria | 4 | kfree en error paths |
| Makefile | 5 | Agregar _tpf _tlazy _tmmap_sim |
| Git | 5 | Commits semánticos |
| Distribución/Roles | 10 | Documentar en README |
| Integración | 5 | P2 + P3 sin conflictos |
| Features avanzadas | 10 | Tests + getpfcount + help |
| **TOTAL** | **100** | |

---

*v2 — Actualizado con inspección real de P2 + Kiro-CLI risk analysis*  
*Council: Claude Sonnet 4.6 (anchor) + Gemini (challenger) + Kiro-CLI (risk validator)*  
*Fecha: 2026-05-10*
