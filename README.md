# Proyecto 2: Comandos EAFITos — Gestion de Memoria en xv6-riscv

> **Sistemas Operativos (SI3003)** — Universidad EAFIT  
> **Autor:** Emmanuel Hernandez  
> **Profesor:** MBA, I.S. Jose Luis Montoya Pareja

---

## Tabla de Contenidos

- [Descripcion](#descripcion)
- [Compilacion y Ejecucion](#compilacion-y-ejecucion)
- [Comandos Implementados](#comandos-implementados)
  - [hello — Primera syscall end-to-end](#hello--primera-syscall-end-to-end)
  - [trace — Mini strace por mascara](#trace--mini-strace-por-mascara)
  - [dumpvm — Explorar tabla de paginas](#dumpvm--explorar-tabla-de-paginas)
  - [memro — Permisos de memoria y page fault](#memro--permisos-de-memoria-y-page-fault)
  - [uargs — Robustez de argumentos](#uargs--robustez-de-argumentos)
- [Memoria Compartida](#memoria-compartida)
- [Archivos Modificados](#archivos-modificados)
  - [Kernel](#kernel)
  - [User](#user)
- [Documentacion Detallada](#documentacion-detallada)

---

## Descripcion

Extension del sistema operativo xv6 (RISC-V) con **5 comandos de gestion de memoria** que demuestran conceptos fundamentales de sistemas operativos:

| Concepto | Comando que lo demuestra |
|----------|--------------------------|
| Path completo de una syscall | `hello` |
| Trazado de syscalls (strace) | `trace` |
| Tabla de paginas Sv39 | `dumpvm` |
| Permisos PTE y page faults | `memro` |
| Robustez user/kernel boundary | `uargs` |

Este proyecto parte de la base del **Proyecto 1** (comandos basicos de shell EAFITos) e incorpora ademas un mecanismo de **memoria compartida** entre procesos.

---

## Compilacion y Ejecucion

```bash
make qemu
```

Dentro de la shell de xv6:

```
$ hello
$ trace 134217728 hello
$ dumpvm
$ memro
$ uargs
$ sharedtest
```

> **Requisitos:** toolchain `riscv64-unknown-elf-gcc`, `qemu-system-riscv64`.

---

## Comandos Implementados

### `hello` — Primera syscall end-to-end

**Archivo:** [`user/hello.c`](user/hello.c)  
**Syscall:** `hello()` → `sys_hello()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_hello = 27`

**Que hace:**  
Ejecuta una syscall completa de punta a punta. El kernel imprime un saludo y retorna 42.

**Path de la syscall:**
```
user/hello.c → hello() stub (ecall) → trap.c usertrap() → syscall.c dispatch → sys_hello() → return 42
```

**Salida esperada:**
```
hello: calling syscall hello()...
[trap] pid 3: ecall from U-mode, scause=8, sepc=0x..., syscall=27
hello: greetings from the xv6 kernel!
hello: hello() returned 42
```

**Concepto demostrado:** el camino completo de una syscall desde user space hasta el kernel y de vuelta: `ecall` → trap → dispatcher → handler → retorno por `a0`.

---

### `trace` — Mini strace por mascara

**Archivo:** [`user/trace.c`](user/trace.c)  
**Syscall:** `trace(int mask)` → `sys_trace()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_trace = 28`

**Que hace:**  
Guarda una bitmask en `struct proc`. Cada vez que el proceso (o sus hijos via `fork()`) ejecuta una syscall cuyo bit este activo en la mascara, el kernel imprime informacion de trazado.

**Uso:**
```
$ trace <mask> <comando> [args...]
```

**Ejemplo — trazar `hello` (bit 27 = `1 << 27 = 134217728`):**
```
$ trace 134217728 hello
```

**Salida esperada:**
```
hello: calling syscall hello()...
hello: greetings from the xv6 kernel!
[trace] pid 4: syscall hello (num=27) args=(...) -> 42
hello: hello() returned 42
```

**Ejemplo — trazar `write` (bit 16 = `1 << 16 = 65536`):**
```
$ trace 65536 echo hola
```

**Implementacion clave** ([`kernel/syscall.c:195`](kernel/syscall.c)):
```c
if(p->trace_mask & (1 << num)) {
    printf("[trace] pid %d: syscall %s (num=%d) args=(...) -> %d\n", ...);
}
```

**Herencia:** en `fork()`, el hijo hereda `trace_mask` del padre ([`kernel/proc.c`](kernel/proc.c)):
```c
np->trace_mask = p->trace_mask;
```

**Concepto demostrado:** trazado de syscalls mediante bitmask, herencia de configuracion en `fork()`.

---

### `dumpvm` — Explorar tabla de paginas

**Archivo:** [`user/dumpvm.c`](user/dumpvm.c)  
**Syscall:** `dumpvm()` → `sys_dumpvm()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_dumpvm = 29`

**Que hace:**  
Llama a `vmprint()` que recorre la tabla de paginas **Sv39 de 3 niveles** del proceso actual. Imprime cada PTE valido con su direccion virtual, direccion fisica y flags de permisos (`R`, `W`, `X`, `U`).

El programa muestra la tabla **antes** y **despues** de `sbrk(8192)` + touch, para visualizar como crecen las paginas del heap.

**Salida esperada:**
```
=== dumpvm: initial page table ===
=== Page table for pid 3 ===
..L2 pte 0: pa=0x... flags=[---]
....L1 pte 0: pa=0x... flags=[---]
......L0 pte 0: pa=0x... flags=[RWXU]   <- text
......L0 pte 1: pa=0x... flags=[RWXU]   <- data
...
=== dumpvm: allocating 8192 bytes with sbrk ===
=== dumpvm: page table after sbrk + touch ===
...                                       <- 2 paginas nuevas del heap
```

**Implementacion clave** ([`kernel/vm.c`](kernel/vm.c) — `vmprint()`):
```
Recorrido de 3 niveles: L2 → L1 → L0
Cada nivel: 512 entradas (9 bits del VA)
Solo imprime PTEs con bit V (Valid) activo
```

**Concepto demostrado:** estructura de la tabla de paginas Sv39, layout virtual del proceso, efecto de `sbrk()` en el address space.

---

### `memro` — Permisos de memoria y page fault

**Archivo:** [`user/memro.c`](user/memro.c)  
**Syscall:** `map_ro(void *va)` → `sys_map_ro()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_map_ro = 30`

**Que hace:**  
1. Mapea una pagina fisica en `va=0x40000000` con permisos **solo lectura** (`PTE_R | PTE_U`, sin `PTE_W`)
2. Copia un mensaje del kernel a la pagina
3. Lee la pagina exitosamente
4. Intenta **escribir** → provoca un **store page fault** (`scause=15`) y el kernel mata al proceso

**Salida esperada:**
```
memro: mapping read-only page at va=0x40000000
memro: map_ro returned 0 (success)
memro: reading from RO page: "Hola desde el kernel xv6 - pagina solo lectura"

memro: now attempting WRITE to read-only page...
memro: this should trigger a store page fault (scause=15)
[trap] pid 3: store page fault at va=0x40000000
```

**Implementacion clave** ([`kernel/sysproc.c`](kernel/sysproc.c)):
```c
// Mapea sin PTE_W → escritura causa page fault
mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_R | PTE_U);
```

**Manejo del fault** ([`kernel/trap.c`](kernel/trap.c)):
```c
} else if(r_scause() == 15) {
    // store page fault → kill process
}
```

**Concepto demostrado:** permisos a nivel de PTE, page faults controlados, proteccion de memoria por hardware.

---

### `uargs` — Robustez de argumentos

**Archivo:** [`user/uargs.c`](user/uargs.c)  
**Syscalls usadas:** `hello()`, `write()`, `read()`, `open()`, `trace()`

**Que hace:**  
Prueba que el kernel maneja correctamente argumentos invalidos **sin causar panic**:

| Test | Llamada | Resultado esperado |
|------|---------|-------------------|
| 1 | `hello()` | Retorna 42 (caso normal) |
| 2 | `write(99, "test", 4)` | Retorna -1 (fd invalido) |
| 3 | `read(99, buf, 16)` | Retorna -1 (fd invalido) |
| 4 | `open(0xdeadbeef, 0)` | Retorna -1 (puntero invalido) |

Usa `trace()` para imprimir los argumentos crudos del trapframe en cada syscall.

**Salida esperada:**
```
=== uargs: syscall argument robustness test ===

--- Test 1: hello() ---
hello: greetings from the xv6 kernel!
[trace] pid 3: syscall hello (num=27) args=(...) -> 42
hello returned: 42

--- Test 2: write(99, ...) invalid fd ---
[trace] pid 3: syscall write (num=16) args=(...) -> -1
write(99) returned: -1 (expected -1)

--- Test 3: read(99, ...) invalid fd ---
[trace] pid 3: syscall read (num=5) args=(...) -> -1
read(99) returned: -1 (expected -1)

--- Test 4: open(0xdeadbeef, 0) invalid pointer ---
[trace] pid 3: syscall open (num=15) args=(...) -> -1
open(0xdeadbeef) returned: -1 (expected -1)

=== uargs: all tests completed without kernel panic ===
```

**Concepto demostrado:** `copyin`/`copyout` validan punteros de usuario, `fileread`/`filewrite` validan file descriptors. El kernel nunca hace panic por argumentos invalidos de user space.

---

## Memoria Compartida

**Programa de prueba:** [`user/sharedtest.c`](user/sharedtest.c)  
**Syscalls:** `setshm()`, `getshm()`  
**Implementacion:** [`kernel/crear_memoria_compartida.c`](kernel/crear_memoria_compartida.c)

Mecanismo de memoria compartida entre procesos padre/hijo:

1. `setshm()` activa la memoria compartida en el proceso
2. `fork()` propaga la pagina compartida al hijo via `mappages()` (misma pagina fisica)
3. `getshm()` retorna la direccion virtual de la pagina compartida
4. Proteccion con `spinlock` y `refcount` para acceso concurrente

**Ejecucion:**
```
$ sharedtest
PADRE (3) addr: 0x...
PADRE (3) escribio: Hola
HIJO (4) addr: 0x...
HIJO (4) lee: Hola
```

### Bug identificado y solucionado

**Problema:** `uvmcopy()` en `fork()` copia **todas** las paginas del padre al hijo, incluyendo la pagina compartida. Esto crea una copia independiente en lugar de compartir la misma pagina fisica.

**Efecto:** padre e hijo escribian en paginas distintas aunque tuvieran la misma direccion virtual → el hijo no veia los datos del padre.

**Solucion:** en `fork()`, la pagina compartida se mapea directamente con `mappages()` apuntando a la misma pagina fisica, en lugar de pasar por `uvmcopy()`.

---

## Archivos Modificados

### Kernel

| Archivo | Cambios |
|---------|---------|
| [`kernel/syscall.h`](kernel/syscall.h) | Numeros de syscall: `hello(27)`, `trace(28)`, `dumpvm(29)`, `map_ro(30)` |
| [`kernel/syscall.c`](kernel/syscall.c) | Dispatch table, nombres de syscalls, logica de tracing |
| [`kernel/sysproc.c`](kernel/sysproc.c) | `sys_hello`, `sys_trace`, `sys_dumpvm`, `sys_map_ro` |
| [`kernel/proc.h`](kernel/proc.h) | Campos: `trace_mask`, `shm_va`, `usar_memoria_compartida` |
| [`kernel/proc.c`](kernel/proc.c) | Herencia de `trace_mask` en `fork()`, cleanup en `freeproc()` |
| [`kernel/trap.c`](kernel/trap.c) | Debug de ecall para syscalls custom, page fault logging |
| [`kernel/vm.c`](kernel/vm.c) | `vmprint()` (recorrido 3 niveles), `ismapped()`, `vmfault()` |
| [`kernel/defs.h`](kernel/defs.h) | Prototipos: `vmprint`, `ismapped`, `vmfault` |
| [`kernel/crear_memoria_compartida.c`](kernel/crear_memoria_compartida.c) | Logica de shared memory con spinlock y refcount |
| [`kernel/crear_memoria_compartida.h`](kernel/crear_memoria_compartida.h) | Header de memoria compartida |

### User

| Archivo | Descripcion |
|---------|-------------|
| [`user/hello.c`](user/hello.c) | Comando 1: prueba syscall hello |
| [`user/trace.c`](user/trace.c) | Comando 2: trazador de syscalls |
| [`user/dumpvm.c`](user/dumpvm.c) | Comando 3: dump de page table |
| [`user/memro.c`](user/memro.c) | Comando 4: pagina read-only + page fault |
| [`user/uargs.c`](user/uargs.c) | Comando 5: robustez de argumentos |
| [`user/sharedtest.c`](user/sharedtest.c) | Prueba de memoria compartida |
| [`user/user.h`](user/user.h) | Prototipos de syscalls nuevas |
| [`user/usys.pl`](user/usys.pl) | Generacion de stubs |
| [`Makefile`](Makefile) | `UPROGS` y `OBJS` actualizados |

---

## Documentacion Detallada

Ver [`DOCUMENTACION.md`](DOCUMENTACION.md) para una explicacion completa archivo por archivo, conceptos teoricos, el path de una syscall, y guia de cada comando con salidas esperadas.
