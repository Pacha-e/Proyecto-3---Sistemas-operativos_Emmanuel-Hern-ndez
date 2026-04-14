# Proyecto 2: Comandos EAFITos — Gestion de Memoria en xv6-riscv

> **Sistemas Operativos (SI3003)** — Universidad EAFIT  
> **Autor:** Emmanuel Hernandez  
> **Profesor:** MBA, I.S. Jose Luis Montoya Pareja

---

## Tabla de Contenidos

1. [Que estamos construyendo](#1-que-estamos-construyendo)
2. [El camino de una syscall](#2-el-camino-de-una-syscall-de-usuario-al-kernel)
3. [Archivo por archivo: que se cambio y por que](#3-archivo-por-archivo-que-se-cambio-y-por-que)
4. [Los 5 comandos de usuario](#4-los-5-comandos-de-usuario)
5. [Memoria compartida](#5-memoria-compartida)
6. [Como compilar y probar](#6-como-compilar-y-probar)

---

## 1. Que estamos construyendo

Agregamos **4 syscalls** nuevas y **5 programas** de usuario:

| Syscall | Numero | Programa | Que hace |
|---------|--------|----------|----------|
| `hello` | 27 | `hello` | Imprime un saludo desde el kernel y devuelve 42 |
| `trace` | 28 | `trace` | Activa rastreo de syscalls (como un espia) |
| `dumpvm` | 29 | `dumpvm` | Muestra la page table del proceso |
| `map_ro` | 30 | `memro` | Mapea una pagina de solo lectura |
| *(combinacion)* | — | `uargs` | Prueba robustez de argumentos |

Ademas se incluye un mecanismo de **memoria compartida** entre procesos (`sharedtest`).

---

## 2. El camino de una syscall: de usuario al kernel

Para entender los cambios, primero hay que entender el camino COMPLETO que recorre una syscall. Usemos `hello()` como ejemplo:

```
PASO 1: Programa de usuario llama hello()
         |
PASO 2: user/user.h tiene el prototipo: int hello(void);
        Esto le dice al compilador que hello() existe y devuelve un int.
         |
PASO 3: user/usys.pl genera el codigo ensamblador que:
        - Pone el numero 27 (SYS_hello) en el registro a7
        - Ejecuta la instruccion ecall
         |
PASO 4: El procesador RISC-V cambia a modo supervisor
        y salta al trap handler (kernel/trap.c -> usertrap())
         |
PASO 5: usertrap() detecta que scause=8 (syscall)
        y llama a syscall() (kernel/syscall.c)
         |
PASO 6: syscall() lee el numero 27 del registro a7,
        busca en la tabla syscalls[27] -> sys_hello
         |
PASO 7: Se ejecuta sys_hello() (kernel/sysproc.c)
        que imprime el mensaje y devuelve 42
         |
PASO 8: syscall() guarda el 42 en trapframe->a0
        (a0 es el registro de retorno)
         |
PASO 9: El kernel devuelve el control al programa
        El programa recibe 42 como valor de retorno de hello()
```

Cada archivo que modificamos corresponde a un eslabon de esta cadena.

---

## 3. Archivo por archivo: que se cambio y por que

### 3.1 kernel/syscall.h — Los numeros de las syscalls

Define constantes numericas para cada syscall. Cuando un programa quiere hacer una syscall, pone un NUMERO en el registro a7 del procesador.

```c
#define SYS_hello  27
#define SYS_trace  28
#define SYS_dumpvm 29
#define SYS_map_ro 30
```

Los numeros 1-26 ya estaban ocupados por las syscalls existentes (fork=1, exit=2, write=16, etc.). Nuestras 4 nuevas syscalls son 27, 28, 29 y 30.

---

### 3.2 kernel/proc.h — La ficha de cada proceso

Define `struct proc` — la estructura que contiene TODA la informacion de un proceso. Se agrego un campo:

```c
int trace_mask;  // mascara de bits para rastreo de syscalls
```

`trace_mask` es un **bitmask** (mascara de bits). Un `int` tiene 32 bits, y cada bit corresponde a un numero de syscall:

```
Bit 0  -> syscall 0 (no existe)
Bit 1  -> syscall 1 (fork)
Bit 16 -> syscall 16 (write)
Bit 27 -> syscall 27 (hello)
```

Para verificar si una syscall N esta en la mascara:
```c
if(mask & (1 << N))  // el bit N esta encendido?
```

Tambien se agrego `map_ro_va` para rastrear la pagina de solo lectura y poder limpiarla cuando el proceso termina.

---

### 3.3 kernel/syscall.c — El despachador de syscalls

Este archivo es el "centro de llamadas" del kernel. Cuando llega una syscall, decide a que funcion enviarla.

**Tabla de despacho:**
```c
static uint64 (*syscalls[])(void) = {
  ...
  [SYS_hello]  sys_hello,
  [SYS_trace]  sys_trace,
  [SYS_dumpvm] sys_dumpvm,
  [SYS_map_ro] sys_map_ro,
};
```

Es un array donde cada posicion contiene la funcion que maneja esa syscall. `syscalls[27]` apunta a `sys_hello`. Cuando llega una syscall con numero `num`, el kernel hace `syscalls[num]()`.

**Logica de rastreo** (despues de ejecutar la syscall):
```c
if(p->trace_mask & (1 << num)) {
  printf("[trace] pid %d: syscall %s (num=%d) args=(...) -> %ld\n", ...);
}
```

El rastreo va DESPUES de ejecutar porque queremos mostrar tambien el valor de retorno.

---

### 3.4 kernel/sysproc.c — Las funciones que hacen el trabajo

**sys_hello():** La syscall mas simple. Imprime un saludo desde el kernel y devuelve 42.
```c
uint64 sys_hello(void) {
  printf("hello: greetings from the xv6 kernel!\n");
  return 42;
}
```

**sys_trace():** Lee la mascara del argumento y la guarda en el proceso.
```c
uint64 sys_trace(void) {
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}
```

**sys_dumpvm():** Llama a `vmprint()` para imprimir la page table del proceso.

**sys_map_ro():** Mapea una pagina de solo lectura. Pasos:
1. `argaddr(0, &va)` — lee la direccion virtual
2. `PGROUNDDOWN(va)` — alinea a inicio de pagina
3. `ismapped()` — verifica que no haya algo ahi
4. `kalloc()` — pide una pagina fisica
5. Copia un mensaje del kernel a la pagina
6. `mappages(..., PTE_R | PTE_U)` — mapea SIN PTE_W (no se puede escribir)

Si el programa intenta escribir -> store page fault (scause=15) y el kernel mata al proceso.

---

### 3.5 kernel/vm.c — vmprint(): ver la page table

Se agrego `vmprint()` que recorre la page table Sv39 de 3 niveles e imprime las entradas validas.

Cada tabla tiene 512 entradas (2^9). La arquitectura Sv39 divide una direccion virtual asi:
- 9 bits (nivel 2) + 9 bits (nivel 1) + 9 bits (nivel 0) + 12 bits (offset)

Para distinguir un puntero de una hoja: si la PTE tiene alguno de R, W o X es una **hoja** (apunta a pagina fisica real). Si no tiene ninguno, es un **puntero** al siguiente nivel.

---

### 3.6 kernel/defs.h — El directorio telefonico del kernel

Se agrego el prototipo de `vmprint` y `ismapped`:
```c
void vmprint(pagetable_t);
int  ismapped(pagetable_t, uint64);
```

Sin estas lineas, `sysproc.c` no podria llamar a estas funciones.

---

### 3.7 kernel/trap.c — El manejador de excepciones

Maneja TODAS las interrupciones y excepciones. La funcion `usertrap()` es el punto de entrada cuando un programa de usuario causa una excepcion.

**Cambios:**
1. Debug print para syscalls del proyecto (>= SYS_hello)
2. Manejo de page faults: intenta resolver con `vmfault()` primero (lazy allocation). Si no puede resolver, imprime diagnostico y mata el proceso.

`r_scause()` indica la causa:
- `8` = syscall (ecall)
- `13` = load page fault
- `15` = store page fault

`epc += 4` es necesario porque queremos volver a la instruccion SIGUIENTE al ecall, no repetirlo.

---

### 3.8 user/user.h — Prototipos para el usuario

```c
int hello(void);
int trace(int);
int dumpvm(void);
int map_ro(void*);
```

### 3.9 user/usys.pl — Generador de stubs

Cada `entry("hello")` genera codigo ensamblador que pone el numero de syscall en a7 y ejecuta `ecall`. Es el puente entre el mundo del usuario y el kernel.

### 3.10 Makefile

Se agregaron los programas a `UPROGS` para que se incluyan en la imagen del disco.

---

## 4. Los 5 comandos de usuario

### 4.1 `hello` — Primera syscall end-to-end

**Archivo:** [`user/hello.c`](user/hello.c)  
**Syscall:** `hello()` -> `sys_hello()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_hello = 27`

Ejecuta una syscall completa de punta a punta. El kernel imprime un saludo y retorna 42.

```
user/hello.c -> hello() stub (ecall) -> trap.c usertrap() -> syscall.c dispatch -> sys_hello() -> return 42
```

**Salida esperada:**
```
hello: calling syscall hello()...
[trap] pid 3: ecall from U-mode, scause=8, sepc=0x..., syscall=27
hello: greetings from the xv6 kernel!
hello: hello() returned 42
```

**Concepto demostrado:** el camino completo de una syscall desde user space hasta el kernel y de vuelta: `ecall` -> trap -> dispatcher -> handler -> retorno por `a0`.

---

### 4.2 `trace` — Mini strace por mascara

**Archivo:** [`user/trace.c`](user/trace.c)  
**Syscall:** `trace(int mask)` -> `sys_trace()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_trace = 28`

Guarda una bitmask en `struct proc`. Cada vez que el proceso (o sus hijos via `fork()`) ejecuta una syscall cuyo bit este activo en la mascara, el kernel imprime informacion de trazado.

**Uso:**
```
$ trace <syscall> <comando> [args...]
```

Se puede usar el **nombre** de la syscall o un numero de mascara:

**Ejemplo — trazar `hello` por nombre:**
```
$ trace hello hello
```

**Salida esperada:**
```
hello: calling syscall hello()...
hello: greetings from the xv6 kernel!
[trace] pid 4: syscall hello (num=27) args=(...) -> 42
hello: hello() returned 42
```

**Ejemplo — trazar `write`:**
```
$ trace write echo hola
```

**Implementacion clave** ([`kernel/syscall.c`](kernel/syscall.c)):
```c
if(p->trace_mask & (1 << num)) {
    printf("[trace] pid %d: syscall %s (num=%d) args=(...) -> %ld\n", ...);
}
```

**Herencia en fork():** el hijo hereda `trace_mask` del padre ([`kernel/proc.c`](kernel/proc.c)):
```c
np->trace_mask = p->trace_mask;
```

**Por que funciona con exec?** Cuando un proceso llama a `exec`, el kernel reemplaza el codigo pero NO borra `struct proc`. Como `trace_mask` vive ahi, sobrevive al exec.

**Concepto demostrado:** trazado de syscalls mediante bitmask, herencia de configuracion en `fork()`.

---

### 4.3 `dumpvm` — Explorar tabla de paginas

**Archivo:** [`user/dumpvm.c`](user/dumpvm.c)  
**Syscall:** `dumpvm()` -> `sys_dumpvm()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_dumpvm = 29`

Llama a `vmprint()` que recorre la tabla de paginas **Sv39 de 3 niveles** del proceso actual. Imprime cada PTE valido con su direccion virtual, direccion fisica y flags de permisos (`R`, `W`, `X`, `U`).

El programa muestra la tabla **antes** y **despues** de `sbrk(8192)` + touch, para visualizar como crecen las paginas del heap.

**Por que "tocar" las paginas?** Porque este xv6 usa **lazy allocation**. Cuando llamas `sbrk(8192)`, el kernel solo aumenta `proc->sz` pero NO asigna paginas. La pagina se asigna cuando el programa la usa por primera vez (genera un page fault que vmfault resuelve).

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

**Concepto demostrado:** estructura de la tabla de paginas Sv39, layout virtual del proceso, efecto de `sbrk()` en el address space.

---

### 4.4 `memro` — Permisos de memoria y page fault

**Archivo:** [`user/memro.c`](user/memro.c)  
**Syscall:** `map_ro(void *va)` -> `sys_map_ro()` en [`kernel/sysproc.c`](kernel/sysproc.c)  
**Numero:** `SYS_map_ro = 30`

Que hace:
1. Mapea una pagina fisica en `va=0x40000000` con permisos **solo lectura** (`PTE_R | PTE_U`, sin `PTE_W`)
2. Copia un mensaje del kernel a la pagina
3. Lee la pagina exitosamente
4. Intenta **escribir** -> provoca un **store page fault** (`scause=15`) y el kernel mata al proceso

**Que pasa internamente?** El procesador ve que la PTE no tiene PTE_W, genera un store page fault. El kernel lo atrapa en `usertrap()`, intenta `vmfault()` pero como la pagina ya esta mapeada (tiene PTE_V), vmfault no puede resolver. Entonces imprime el diagnostico y mata el proceso.

**Salida esperada:**
```
memro: mapping read-only page at va=0x40000000
memro: map_ro returned 0 (success)
memro: reading from RO page: "Hola desde el kernel xv6 - pagina solo lectura"

memro: now attempting WRITE to read-only page...
memro: this should trigger a store page fault (scause=15)
[trap] pid 3: store page fault at va=0x40000000
```

**Bug solucionado:** al principio, cuando el proceso de memro terminaba, `freewalk` hacia panic porque la pagina en 0x40000000 quedaba huerfana (estaba mas alla de `p->sz`). Se soluciono guardando la VA en `p->map_ro_va` y haciendo `uvmunmap` en `freeproc()` antes de liberar la page table.

**Concepto demostrado:** permisos a nivel de PTE, page faults controlados, proteccion de memoria por hardware.

---

### 4.5 `uargs` — Robustez de argumentos

**Archivo:** [`user/uargs.c`](user/uargs.c)  
**Syscalls usadas:** `hello()`, `write()`, `read()`, `open()`, `trace()`

Prueba que el kernel maneja correctamente argumentos invalidos **sin causar panic**:

| Test | Llamada | Resultado esperado |
|------|---------|-------------------|
| 1 | `hello()` | Retorna 42 (caso normal) |
| 2 | `write(99, "test", 4)` | Retorna -1 (fd invalido) |
| 3 | `read(99, buf, 16)` | Retorna -1 (fd invalido) |
| 4 | `open(0xdeadbeef, 0)` | Retorna -1 (puntero invalido) |

Usa `trace()` para imprimir los argumentos crudos del trapframe en cada syscall.

- `write(99, ...)`: el fd 99 no existe, el kernel debe devolver -1
- `read(99, ...)`: idem
- `open(0xdeadbeef, ...)`: la direccion 0xdeadbeef es basura, `fetchstr` falla al intentar leer el string

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

## 5. Memoria compartida

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

### Bug que se identifico y soluciono

**Problema:** `uvmcopy()` en `fork()` copia **todas** las paginas del padre al hijo, incluyendo la pagina compartida. Esto crea una copia independiente en lugar de compartir la misma pagina fisica.

**Efecto:** padre e hijo escribian en paginas distintas aunque tuvieran la misma direccion virtual -> el hijo no veia los datos del padre.

**Solucion:** en `fork()`, la pagina compartida se mapea directamente con `mappages()` apuntando a la misma pagina fisica, en lugar de pasar por `uvmcopy()`.

---

## 6. Como compilar y probar

### Requisitos

- Toolchain `riscv64-unknown-elf-gcc`
- `qemu-system-riscv64`

### Compilar y ejecutar

```bash
make clean
make qemu
```

Si todo compila sin errores, veras el prompt de xv6: `$`

### Probar cada comando

```bash
$ hello
# Esperado: mensaje del kernel + "returned 42"

$ trace hello hello
# Esperado: lo mismo pero con lineas [trace]

$ trace write echo hola
# Esperado: traza de la syscall write

$ dumpvm
# Esperado: dos dumps de page table, el segundo con mas paginas

$ memro
# Esperado: lee OK, luego page fault al intentar escribir

$ uargs
# Esperado: todos los tests pasan, sin kernel panic

$ sharedtest
# Esperado: padre escribe, hijo lee el mismo dato

$ ayuda
# Muestra todos los comandos disponibles
```

### Errores comunes

- **"unknown sys call 27"**: falta agregar `sys_hello` a la tabla `syscalls[]` en syscall.c
- **"undefined reference to vmprint"**: falta la declaracion en defs.h
- **"panic: freewalk: leaf"**: hay paginas mapeadas fuera de `p->sz` que no se limpiaron en `freeproc()`
- **Programa no aparece en la shell**: falta la entrada en UPROGS del Makefile

---

## Archivos modificados

### Kernel

| Archivo | Cambios |
|---------|---------|
| [`kernel/syscall.h`](kernel/syscall.h) | Numeros de syscall: `hello(27)`, `trace(28)`, `dumpvm(29)`, `map_ro(30)` |
| [`kernel/syscall.c`](kernel/syscall.c) | Tabla de despacho, nombres de syscalls, logica de tracing |
| [`kernel/sysproc.c`](kernel/sysproc.c) | `sys_hello`, `sys_trace`, `sys_dumpvm`, `sys_map_ro` |
| [`kernel/proc.h`](kernel/proc.h) | Campos: `trace_mask`, `shm_va`, `usar_memoria_compartida`, `map_ro_va` |
| [`kernel/proc.c`](kernel/proc.c) | Herencia de `trace_mask` en `fork()`, cleanup de map_ro en `freeproc()` |
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
