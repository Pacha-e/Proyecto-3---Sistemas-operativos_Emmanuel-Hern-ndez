# Proyecto 2: Comandos EAFITos — Documentación Completa

## Tabla de Contenidos

1. [¿Qué es xv6 y por qué importa?](#1-qué-es-xv6-y-por-qué-importa)
2. [Conceptos fundamentales](#2-conceptos-fundamentales)
3. [¿Qué estamos construyendo?](#3-qué-estamos-construyendo)
4. [El camino de una syscall: de usuario al kernel](#4-el-camino-de-una-syscall-de-usuario-al-kernel)
5. [Archivo por archivo: qué se cambió y por qué](#5-archivo-por-archivo-qué-se-cambió-y-por-qué)
6. [Los 5 comandos de usuario](#6-los-5-comandos-de-usuario)
7. [Cómo probar todo](#7-cómo-probar-todo)

---

## 1. ¿Qué es xv6 y por qué importa?

xv6 es un **sistema operativo de enseñanza** creado por el MIT. Es una versión simplificada de Unix que corre sobre la arquitectura RISC-V (un tipo de procesador, como lo son Intel o ARM, pero de código abierto).

Piénsalo así: Windows, Linux y macOS son sistemas operativos "reales" con millones de líneas de código. xv6 tiene unas ~6,000 líneas y hace lo esencial: maneja procesos, memoria y archivos. Es como un auto de fórmula 1 en miniatura: tiene motor, ruedas y dirección, pero sin aire acondicionado ni GPS.

**¿Por qué modificarlo?** Porque la mejor forma de entender cómo funciona un sistema operativo es metiéndole mano. Nuestro proyecto agrega 4 nuevas "llamadas al sistema" (syscalls) y 5 programas que las usan.

---

## 2. Conceptos fundamentales

### 2.1 ¿Qué es el kernel?

El **kernel** (núcleo) es el corazón del sistema operativo. Es el programa que tiene control total del hardware: la memoria, el procesador, los discos, etc.

Los programas normales (como `ls`, `cat`, o los que escribimos nosotros) corren en **modo usuario** — un modo restringido donde NO pueden acceder directamente al hardware. Es como ser un pasajero en un avión: puedes pedir cosas, pero no puedes tocar los controles.

El kernel corre en **modo supervisor** — tiene acceso total. Es el piloto.

### 2.2 ¿Qué es una syscall (llamada al sistema)?

Cuando un programa de usuario necesita algo que solo el kernel puede hacer (leer un archivo, crear un proceso, pedir memoria), hace una **syscall**. Es como el botón de llamar a la azafata en el avión.

Ejemplo cotidiano:
```
Programa: "Quiero escribir en la pantalla"
   ↓ (syscall write)
Kernel: "Ok, yo me encargo de hablar con el hardware de video"
   ↓
Pantalla: muestra el texto
```

En RISC-V, la instrucción especial que hace esto se llama `ecall`. Cuando un programa ejecuta `ecall`, el procesador automáticamente:
1. Cambia de modo usuario a modo supervisor
2. Salta a una dirección especial del kernel (el "trap handler")
3. El kernel atiende el pedido
4. Devuelve el control al programa

### 2.3 ¿Qué es la memoria virtual y las page tables?

Cada programa cree que tiene TODA la memoria para él solo, empezando desde la dirección 0. Pero esto es una ilusión creada por el kernel usando **page tables** (tablas de páginas).

La memoria se divide en **páginas** de 4096 bytes (4 KB). El kernel mantiene una tabla que dice:

```
"Cuando el programa pide la página virtual 0 → dale la página física 500"
"Cuando el programa pide la página virtual 1 → dale la página física 203"
...etc
```

RISC-V usa un esquema llamado **Sv39** con **3 niveles** de tablas de páginas, como un directorio jerárquico:

```
Nivel 2 (raíz) → apunta a tablas de Nivel 1
  Nivel 1 → apunta a tablas de Nivel 0
    Nivel 0 → apunta a la página física real
```

Cada entrada en la tabla (llamada **PTE**, Page Table Entry) tiene **flags** (banderas) que controlan los permisos:
- **PTE_V** (Valid): la entrada es válida
- **PTE_R** (Read): se puede leer
- **PTE_W** (Write): se puede escribir
- **PTE_X** (Execute): se puede ejecutar como código
- **PTE_U** (User): accesible desde modo usuario

Si un programa intenta escribir en una página que solo tiene permiso de lectura (R pero no W), el procesador genera un **page fault** — una excepción que el kernel atrapa y maneja.

### 2.4 ¿Qué es un proceso?

Un **proceso** es un programa en ejecución. Cada proceso tiene:
- Su propio espacio de memoria (su propia page table)
- Un **PID** (Process ID): un número que lo identifica
- Un **trapframe**: una estructura donde se guardan los registros del procesador cuando el proceso entra al kernel
- Estado: si está corriendo, dormido, zombie, etc.

En xv6, toda la información de un proceso está en una estructura llamada `struct proc` (definida en `proc.h`).

### 2.5 ¿Qué es C?

C es el lenguaje de programación en el que está escrito xv6 (y Linux, y Windows, y casi todo sistema operativo). Algunas cosas básicas que aparecen en el código:

- `int` = número entero
- `uint64` = número entero sin signo de 64 bits (muy grande)
- `char *` = puntero a texto (string)
- `void` = "nada" (una función que no devuelve nada, o que no recibe parámetros)
- `struct` = una agrupación de datos (como una ficha con varios campos)
- `#define` = darle un nombre a un valor constante
- `#include` = importar definiciones de otro archivo
- `->` = acceder a un campo de una estructura a través de un puntero
- `&` = obtener la dirección de memoria de algo
- `(1 << N)` = el número 1 desplazado N posiciones en binario (equivale a 2^N)

---

## 3. ¿Qué estamos construyendo?

Agregamos **4 syscalls** nuevas y **5 programas** de usuario:

| Syscall | Número | Programa | ¿Qué hace? |
|---------|--------|----------|-------------|
| `hello` | 27 | `thello` | Imprime un saludo desde el kernel y devuelve 42 |
| `trace` | 28 | `ttrace` | Activa rastreo de syscalls (como un espía) |
| `dumpvm` | 29 | `tdumpvm` | Muestra la page table del proceso |
| `map_ro` | 30 | `tmemro` | Mapea una página de solo lectura |
| *(combinación)* | — | `tuargs` | Prueba robustez de argumentos |

---

## 4. El camino de una syscall: de usuario al kernel

Para entender los cambios, primero hay que entender el camino COMPLETO que recorre una syscall. Usemos `hello()` como ejemplo:

```
PASO 1: Programa de usuario llama hello()
         ↓
PASO 2: user/user.h tiene el prototipo: int hello(void);
        Esto le dice al compilador que hello() existe y devuelve un int.
         ↓
PASO 3: user/usys.pl genera el código ensamblador que:
        - Pone el número 27 (SYS_hello) en el registro a7
        - Ejecuta la instrucción ecall
         ↓
PASO 4: El procesador RISC-V cambia a modo supervisor
        y salta al trap handler (kernel/trap.c → usertrap())
         ↓
PASO 5: usertrap() detecta que scause=8 (syscall)
        y llama a syscall() (kernel/syscall.c)
         ↓
PASO 6: syscall() lee el número 27 del registro a7,
        busca en la tabla syscalls[27] → sys_hello
         ↓
PASO 7: Se ejecuta sys_hello() (kernel/sysproc.c)
        que imprime el mensaje y devuelve 42
         ↓
PASO 8: syscall() guarda el 42 en trapframe->a0
        (a0 es el registro de retorno)
         ↓
PASO 9: El kernel devuelve el control al programa
        El programa recibe 42 como valor de retorno de hello()
```

Cada archivo que modificamos corresponde a un eslabón de esta cadena.

---

## 5. Archivo por archivo: qué se cambió y por qué

### 5.1 kernel/syscall.h — Los números de las syscalls

**¿Qué es este archivo?**
Define constantes numéricas para cada syscall. Cuando un programa quiere hacer una syscall, pone un NÚMERO en el registro a7 del procesador. Este archivo asigna esos números.

**¿Qué se agregó?**
```c
#define SYS_hello  27
#define SYS_trace  28
#define SYS_dumpvm 29
#define SYS_map_ro 30
```

**¿Por qué?**
Cada syscall necesita un número único. Los números 1-26 ya estaban ocupados por las syscalls existentes (fork=1, exit=2, write=16, etc.). Nuestras 4 nuevas syscalls son 27, 28, 29 y 30.

`#define` en C es como crear un alias: cada vez que el compilador ve `SYS_hello`, lo reemplaza por `27`. Esto hace el código más legible que escribir números "mágicos" por todos lados.

---

### 5.2 kernel/proc.h — La ficha de cada proceso

**¿Qué es este archivo?**
Define `struct proc` — la estructura que contiene TODA la información de un proceso. Es como la ficha médica de un paciente: tiene su estado, su memoria, sus archivos abiertos, etc.

**¿Qué se agregó?**
```c
int trace_mask;  // bitmask for syscall tracing
```

Se agregó UN solo campo nuevo dentro de `struct proc`, justo después de `usar_memoria_compartida`.

**¿Por qué?**
El comando `ttrace` necesita recordar qué syscalls rastrear para cada proceso. `trace_mask` es un **bitmask** (máscara de bits).

¿Qué es un bitmask? Es un número donde cada BIT individual significa algo. Un `int` tiene 32 bits, y cada bit corresponde a un número de syscall:

```
Bit 0  → syscall 0 (no existe)
Bit 1  → syscall 1 (fork)
Bit 2  → syscall 2 (exit)
...
Bit 16 → syscall 16 (write)
...
Bit 27 → syscall 27 (hello)
```

Si quieres rastrear write (16) y hello (27), calculas:
```
mask = (1 << 16) | (1 << 27) = 65536 | 134217728 = 134283264
```

Para verificar si una syscall N está en la máscara:
```c
if(mask & (1 << N))  // ¿el bit N está encendido?
```

Esto es MUCHO más eficiente que guardar una lista de números. Un solo `int` puede representar cualquier combinación de las 32 syscalls.

**¿Por qué ponerlo en struct proc?**
Porque cada proceso puede tener su propia configuración de rastreo. Si el proceso A quiere rastrear `write` pero el proceso B no, cada uno tiene su propio `trace_mask`.

---

### 5.3 kernel/syscall.c — El despachador de syscalls

**¿Qué es este archivo?**
Es el "centro de llamadas" del kernel. Cuando llega una syscall, este archivo decide a qué función enviarla. Es como la recepcionista que recibe la llamada y la transfiere al departamento correcto.

**Cambio 1: Declaraciones extern**
```c
extern uint64 sys_hello(void);
extern uint64 sys_trace(void);
extern uint64 sys_dumpvm(void);
extern uint64 sys_map_ro(void);
```

`extern` le dice al compilador: "estas funciones existen, pero están definidas en OTRO archivo (sysproc.c)". Es como decir "el Dr. García trabaja aquí, pero su consultorio está en el piso 3". Sin esto, el compilador daría error porque no encuentra las funciones.

**Cambio 2: Tabla de despacho**
```c
static uint64 (*syscalls[])(void) = {
  ...
  [SYS_hello]  sys_hello,
  [SYS_trace]  sys_trace,
  [SYS_dumpvm] sys_dumpvm,
  [SYS_map_ro] sys_map_ro,
};
```

Esta es una **tabla de punteros a funciones**. Suena intimidante, pero es simple:
- Es un array (lista) donde la posición indica el número de syscall
- Cada posición contiene la dirección de la función que maneja esa syscall
- `syscalls[27]` → apunta a `sys_hello`
- `syscalls[28]` → apunta a `sys_trace`

Cuando llega una syscall con número `num`, el kernel simplemente hace `syscalls[num]()` — busca en la tabla y llama a la función. Esto es O(1): no importa cuántas syscalls haya, siempre tarda lo mismo.

La sintaxis `[SYS_hello] sys_hello` es una inicialización designada de C: "en la posición SYS_hello (27), pon sys_hello".

**Cambio 3: Array de nombres**
```c
static char *syscall_names[] = {
  [SYS_fork]  "fork",
  [SYS_hello] "hello",
  ...
};
```

Un array paralelo que asocia cada número de syscall con su nombre como texto. Se usa solo para imprimir mensajes legibles en el rastreo. Sin esto, el trace mostraría "syscall 27" en vez de "syscall hello".

**Cambio 4: Lógica de rastreo en syscall()**
```c
void syscall(void) {
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;    // ← lee qué syscall pidió el programa
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();   // ← ejecuta la syscall

    // NUEVO: si esta syscall está en la máscara de rastreo, imprime info
    if(p->trace_mask & (1 << num)) {
      printf("[trace] pid %d: syscall %s (num=%d) ... -> %d\n", ...);
    }
  } else {
    printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

El flujo es:
1. `myproc()` obtiene el proceso actual
2. `p->trapframe->a7` contiene el número de syscall que puso el programa antes de hacer `ecall`
3. Verifica que el número sea válido y que exista un handler
4. Ejecuta la syscall y guarda el resultado en `a0` (el registro de retorno)
5. **NUEVO**: después de ejecutar, revisa si el bit correspondiente está encendido en `trace_mask`. Si sí, imprime información de depuración.

**¿Por qué el rastreo va DESPUÉS de ejecutar?** Porque queremos mostrar también el valor de retorno. Si lo hiciéramos antes, no sabríamos qué devolvió la syscall.

---

### 5.4 kernel/sysproc.c — Las funciones que hacen el trabajo

**¿Qué es este archivo?**
Contiene las implementaciones de syscalls relacionadas con procesos y sistema. Aquí es donde vive la lógica real de cada syscall.

**Función 1: sys_hello()**
```c
uint64 sys_hello(void) {
  printf("hello: greetings from the xv6 kernel!\n");
  return 42;
}
```

La syscall más simple posible. Demuestra que podemos:
- Ejecutar código dentro del kernel (el `printf` aquí es el printf DEL KERNEL, no el del usuario)
- Devolver un valor al programa de usuario (42)

`return 42` pone el valor 42 en `trapframe->a0`, que es lo que el programa de usuario recibe como valor de retorno de `hello()`.

**Función 2: sys_trace()**
```c
uint64 sys_trace(void) {
  int mask;
  argint(0, &mask);           // lee el primer argumento
  myproc()->trace_mask = mask; // lo guarda en el proceso
  return 0;
}
```

- `argint(0, &mask)`: lee el argumento número 0 (el primero) de la syscall. Los argumentos de las syscalls viajan en los registros a0, a1, a2... del trapframe. `argint` lee a0 y lo guarda en la variable `mask`.
- `myproc()->trace_mask = mask`: guarda la máscara en el campo que agregamos a struct proc.

A partir de este momento, cada vez que este proceso haga una syscall, la función `syscall()` (en syscall.c) revisará este campo y decidirá si imprimir o no.

**¿Por qué funciona con exec?** Cuando un proceso llama a `exec` (para ejecutar otro programa), el kernel reemplaza el código del proceso pero NO borra `struct proc`. Como `trace_mask` vive ahí, sobrevive al `exec`. Así, `ttrace` puede hacer `trace(mask)` y luego `exec(comando)`, y el comando heredará la máscara.

**Función 3: sys_dumpvm()**
```c
uint64 sys_dumpvm(void) {
  struct proc *p = myproc();
  printf("=== Page table for pid %d ===\n", p->pid);
  vmprint(p->pagetable);     // llama a la función que recorre la page table
  return 0;
}
```

Obtiene la page table del proceso actual y llama a `vmprint()` (que definimos en vm.c) para imprimirla. `p->pagetable` es un puntero a la tabla de nivel 2 (la raíz) de la page table del proceso.

**Función 4: sys_map_ro()**
```c
uint64 sys_map_ro(void) {
  uint64 va;
  argaddr(0, &va);            // lee la dirección virtual deseada
  struct proc *p = myproc();

  va = PGROUNDDOWN(va);       // alinea a inicio de página

  if(ismapped(p->pagetable, va))
    return -1;                 // error si ya hay algo mapeado ahí

  char *mem = kalloc();        // pide una página física al kernel
  if(mem == 0)
    return -1;                 // error si no hay memoria
  memset(mem, 0, PGSIZE);     // llena de ceros

  // Escribe un mensaje en la página
  char *msg = "Hola desde el kernel xv6 - pagina solo lectura";
  memmove(mem, msg, strlen(msg) + 1);

  // Mapea la página con permisos de SOLO LECTURA
  if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_R | PTE_U) != 0) {
    kfree(mem);
    return -1;
  }
  return 0;
}
```

Esta es la syscall más compleja. Paso a paso:

1. **`argaddr(0, &va)`**: lee el primer argumento como dirección. El programa de usuario le pasó `0x40000000` como la dirección donde quiere la página.

2. **`PGROUNDDOWN(va)`**: alinea la dirección al inicio de la página (múltiplo de 4096). Las páginas SIEMPRE empiezan en múltiplos de 4096.

3. **`ismapped()`**: verifica que no haya ya algo mapeado en esa dirección. Si lo hay, devuelve error (-1) para no pisar memoria existente.

4. **`kalloc()`**: pide al asignador de memoria del kernel una página física libre (4096 bytes). Es como ir al almacén y pedir una hoja en blanco.

5. **`memset(mem, 0, PGSIZE)`**: llena toda la página de ceros (limpia la hoja).

6. **`memmove(mem, msg, ...)`**: copia el mensaje "Hola desde el kernel..." en la página. Ahora la página tiene contenido.

7. **`mappages(..., PTE_R | PTE_U)`**: este es el paso clave. Crea una entrada en la page table del proceso que dice:
   - "La dirección virtual `0x40000000`..."
   - "...apunta a la página física que acabamos de llenar..."
   - "...con permisos `PTE_R | PTE_U`"
   
   Los permisos son:
   - `PTE_R` = se puede LEER
   - `PTE_U` = accesible desde modo Usuario
   - **NO hay PTE_W** = NO se puede escribir
   
   `mappages` agrega automáticamente `PTE_V` (válido).

**¿Qué pasa cuando el programa intenta ESCRIBIR en esa página?**
El procesador ve que la PTE no tiene PTE_W, genera un **store page fault** (scause=15), el kernel lo atrapa en `usertrap()`, y como no puede resolver el fault (la página SÍ está mapeada pero no es escribible), mata al proceso. Esto es exactamente lo que demuestra `tmemro`.

---

### 5.5 kernel/vm.c — vmprint(): ver la page table

**¿Qué es este archivo?**
Contiene todo el código de manejo de memoria virtual: crear page tables, mapear páginas, copiar memoria entre procesos, etc.

**¿Qué se agregó?**
La función `vmprint()` al final del archivo. Todo el código existente queda intacto.

```c
void vmprint(pagetable_t pagetable) {
  // Recorre los 512 entries del nivel 2
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){  // solo si la entrada es válida
      // Imprime la entrada de nivel 2
      // Si no tiene R/W/X, es un puntero a nivel 1
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // Recorre las 512 entries del nivel 1
        for(int j = 0; j < 512; j++){
          // Si no tiene R/W/X, es un puntero a nivel 0
          if(...){
            // Recorre las 512 entries del nivel 0
            // Estas SÍ son hojas (apuntan a páginas reales)
          }
        }
      }
    }
  }
}
```

**¿Por qué 512?** Cada tabla de páginas tiene 512 entradas (2^9). Esto viene de la arquitectura Sv39:
- Una dirección virtual tiene 39 bits útiles
- Se dividen en: 9 bits (nivel 2) + 9 bits (nivel 1) + 9 bits (nivel 0) + 12 bits (offset dentro de la página)
- 2^9 = 512 entradas por nivel

**¿Cómo distinguir un puntero de una hoja?**
- Si una PTE tiene alguno de R, W o X → es una **hoja** (apunta a una página física real)
- Si no tiene ninguno de R, W, X → es un **puntero** a la tabla del siguiente nivel

**`PTE2PA(pte)`**: macro que extrae la dirección física de una PTE. Las PTEs almacenan la dirección física desplazada y con flags, esta macro limpia los flags y reconstruye la dirección real.

La salida se ve algo así:
```
..0: pte 0x21fda801 pa 0x87f6a000 [----]        ← nivel 2, puntero
.. ..0: pte 0x21fda401 pa 0x87f69000 [----]      ← nivel 1, puntero
.. .. ..0: pte 0x21fdb81f pa 0x87f6e000 [RWXU]   ← nivel 0, hoja (código+datos)
.. .. ..1: pte 0x21fdb41f pa 0x87f6d000 [RWXU]   ← otra página
```

---

### 5.6 kernel/defs.h — El directorio telefónico del kernel

**¿Qué es este archivo?**
Contiene las **declaraciones** (prototipos) de TODAS las funciones del kernel. Es como el directorio telefónico: le dice a cada archivo qué funciones existen en otros archivos.

En C, si un archivo A quiere llamar a una función definida en un archivo B, necesita saber que esa función existe y qué parámetros recibe. `defs.h` proporciona esa información para todo el kernel.

**¿Qué se agregó?**
```c
void            vmprint(pagetable_t);
```

Una sola línea en la sección `// vm.c`. Le dice al resto del kernel: "existe una función `vmprint` que recibe una page table y no devuelve nada". Sin esta línea, `sysproc.c` no podría llamar a `vmprint()` y el compilador daría error.

---

### 5.7 kernel/trap.c — El manejador de excepciones

**¿Qué es este archivo?**
Maneja TODAS las interrupciones y excepciones del procesador. Cuando algo "inusual" pasa (una syscall, un page fault, una interrupción de timer, etc.), el procesador salta aquí.

La función principal es `usertrap()` — el punto de entrada cuando un programa de usuario causa una excepción.

**Cambio 1: #include "syscall.h"**
```c
#include "syscall.h"
```

Necesario para poder usar `SYS_hello` en el código de depuración. Sin esto, el compilador no sabría qué es `SYS_hello`.

**Cambio 2: Debug print para syscalls custom**
```c
if(r_scause() == 8){
  // ... código existente ...
  p->trapframe->epc += 4;

  // NUEVO: debug para nuestras syscalls
  int sysnum = p->trapframe->a7;
  if(sysnum >= SYS_hello) {
    printf("[trap] pid %d: ecall from U-mode, scause=8, sepc=0x%lx, syscall=%d\n",
      p->pid, r_sepc(), sysnum);
  }
  // ... resto existente ...
}
```

**¿Qué es `r_scause()`?** Lee el registro `scause` del procesador, que indica la CAUSA de la excepción:
- `scause = 8` → syscall (instrucción ecall desde modo usuario)
- `scause = 13` → load page fault (intentó LEER una dirección inválida)
- `scause = 15` → store page fault (intentó ESCRIBIR en una dirección inválida o sin permiso de escritura)

**¿Por qué `epc += 4`?** Cuando ocurre una syscall, `sepc` (saved exception program counter) apunta a la instrucción `ecall`. Pero cuando volvamos al programa, queremos ejecutar la instrucción SIGUIENTE, no repetir el `ecall`. Como cada instrucción RISC-V mide 4 bytes, sumamos 4.

**¿Por qué solo imprimir para syscalls >= SYS_hello?** Para no llenar la pantalla con mensajes de syscalls normales (fork, write, read se llaman constantemente). Solo nos interesan las nuestras para depuración.

**Cambio 3: Manejo de page faults separado**

Código original:
```c
} else if((r_scause() == 15 || r_scause() == 13) &&
          vmfault(p->pagetable, r_stval(), ...) != 0) {
  // page fault on lazily-allocated page — OK, resuelto
} else {
  // excepción desconocida — matar proceso
}
```

Código nuevo:
```c
} else if(r_scause() == 15 || r_scause() == 13) {
  uint64 va = r_stval();
  uint64 result = vmfault(p->pagetable, va, (r_scause() == 13) ? 1 : 0);
  if(result == 0) {
    printf("[trap] pid %d: %s page fault at va=0x%lx, scause=%ld - killing process\n",
      p->pid,
      (r_scause() == 15) ? "store" : "load",
      va, r_scause());
    setkilled(p);
  }
}
```

**¿Qué es `r_stval()`?** Lee el registro `stval` que contiene la dirección virtual que causó el page fault. Es decir, la dirección que el programa intentó acceder y no pudo.

**¿Qué es `vmfault()`?** Es una función existente que intenta resolver page faults de **lazy allocation**. En este xv6 modificado, cuando un programa pide memoria con `sbrk()`, el kernel NO asigna páginas de inmediato — solo aumenta el tamaño lógico. La página se asigna cuando el programa realmente la usa (lazy = perezoso). `vmfault` hace esa asignación tardía.

**¿Por qué separar el manejo?** El código original combinaba la verificación y el manejo en una sola condición. Pero necesitamos distinguir dos casos:
1. **vmfault resuelve el fault** (devuelve != 0): todo bien, era lazy allocation
2. **vmfault NO puede resolver** (devuelve 0): puede ser porque la página ya está mapeada pero es de solo lectura (caso de `tmemro`). En este caso, imprimimos un mensaje informativo y matamos el proceso.

**¿Por qué vmfault devuelve 0 para la página de tmemro?** Porque `vmfault` primero verifica `ismapped()`. La página de `map_ro` SÍ está mapeada (tiene PTE_V), así que `ismapped` devuelve 1, y `vmfault` devuelve 0 ("no hice nada, ya está mapeada"). Pero el page fault ocurrió porque la página no tiene PTE_W. Como vmfault no resolvió nada, llegamos al `if(result == 0)` y matamos el proceso. Exactamente el comportamiento deseado.

---

### 5.8 user/user.h — Los prototipos para el usuario

**¿Qué es este archivo?**
El equivalente de `defs.h` pero para programas de usuario. Declara todas las funciones que los programas pueden usar: syscalls, funciones de utilidad (printf, malloc, etc.).

**¿Qué se agregó?**
```c
int hello(void);
int trace(int);
int dumpvm(void);
int map_ro(void*);
```

Cuatro prototipos que le dicen al compilador:
- `hello()` no recibe argumentos, devuelve un int
- `trace(int)` recibe un entero (la máscara), devuelve un int
- `dumpvm()` no recibe argumentos, devuelve un int
- `map_ro(void*)` recibe un puntero (la dirección virtual), devuelve un int

Sin estos prototipos, si un programa intenta llamar `hello()`, el compilador diría "no sé qué es hello".

---

### 5.9 user/usys.pl — El generador de stubs

**¿Qué es este archivo?**
Es un script en Perl que GENERA código ensamblador. Para cada syscall, genera un "stub" — un pedacito de código que prepara los registros y ejecuta `ecall`.

**¿Qué se agregó?**
```perl
entry("hello");
entry("trace");
entry("dumpvm");
entry("map_ro");
```

Cada `entry("hello")` genera algo como:
```assembly
.global hello        # hacer la función visible globalmente
hello:               # etiqueta de la función
  li a7, SYS_hello   # cargar 27 en el registro a7
  ecall               # instrucción que cambia a modo supervisor
  ret                 # retornar al llamador
```

Este es el PUENTE entre el mundo del usuario y el kernel. Cuando un programa llama `hello()`, en realidad ejecuta este código ensamblador que:
1. Pone el número de syscall en a7 (los argumentos ya están en a0-a5 por convención de llamada de C)
2. Ejecuta `ecall` que transfiere control al kernel
3. Cuando el kernel devuelve control, `ret` regresa al programa

**¿Por qué Perl?** Porque generar este código a mano para cada syscall sería tedioso y repetitivo. El script lo automatiza.

---

### 5.10 Makefile — La receta de compilación

**¿Qué es este archivo?**
El Makefile le dice a `make` cómo compilar todo el proyecto. Es como una receta de cocina: "para hacer el kernel, compila estos archivos .c; para hacer el disco, empaqueta estos programas".

**¿Qué se agregó?**
```makefile
$U/_thello\
$U/_ttrace\
$U/_tdumpvm\
$U/_tmemro\
$U/_tuargs\
```

En la lista `UPROGS` (User Programs). Esto le dice a `make`:
1. Compila `user/thello.c` → `user/thello.o`
2. Enlázalo con las librerías de usuario → `user/_thello`
3. Incluye `_thello` en la imagen del disco (`fs.img`)

Sin estas líneas, los programas se compilarían pero NO estarían disponibles dentro de xv6.

El `\` al final de cada línea es una continuación de línea (el Makefile trata todo como una sola línea lógica).

---

## 6. Los 5 comandos de usuario

### 6.1 thello — "Hola desde el kernel"

```c
int main(void) {
  printf("thello: calling syscall hello()...\n");
  int ret = hello();
  printf("thello: hello() returned %d\n", ret);
  exit(0);
}
```

**Propósito**: Demostrar que la syscall funciona end-to-end (de punta a punta).

**Salida esperada**:
```
thello: calling syscall hello()...
hello: greetings from the xv6 kernel!     ← esto lo imprime el KERNEL
thello: hello() returned 42               ← esto lo imprime el USUARIO
```

La primera y tercera líneas vienen del programa de usuario. La segunda viene del kernel. Esto demuestra que cruzamos exitosamente la frontera usuario↔kernel.

### 6.2 ttrace — Mini strace

```c
int main(int argc, char *argv[]) {
  if(argc < 3){
    printf("Usage: ttrace <mask> <command> [args...]\n");
    exit(1);
  }
  int mask = atoi(argv[1]);   // convierte "134217728" a número
  trace(mask);                 // activa el rastreo
  exec(argv[2], &argv[2]);    // reemplaza este programa por el comando dado
  // Si llegamos aquí, exec falló
  printf("ttrace: exec %s failed\n", argv[2]);
  exit(1);
}
```

**Propósito**: Funciona como `strace` en Linux — muestra qué syscalls hace un programa.

**¿Cómo funciona?**
1. Recibe una máscara y un comando por argumentos
2. Llama `trace(mask)` que guarda la máscara en `proc->trace_mask`
3. Llama `exec()` que REEMPLAZA el programa actual por el comando dado
4. Como `trace_mask` sobrevive a `exec`, el nuevo programa hereda el rastreo

**Ejemplo**: `ttrace 134217728 thello`
- 134217728 = 2^27 = (1 << 27) = bit de SYS_hello
- Ejecuta `thello` pero con rastreo activado para hello

**Salida esperada**:
```
thello: calling syscall hello()...
hello: greetings from the xv6 kernel!
[trace] pid 4: syscall hello (num=27) args=(...) -> 42
thello: hello() returned 42
```

### 6.3 tdumpvm — Visualizar la page table

```c
int main(void) {
  printf("=== tdumpvm: initial page table ===\n");
  dumpvm();                    // muestra page table inicial

  printf("\n=== tdumpvm: allocating 8192 bytes with sbrk ===\n");
  char *p = sbrk(8192);       // pide 2 páginas de memoria
  p[0] = 'A';                 // toca la primera página
  p[4096] = 'B';              // toca la segunda página

  printf("\n=== tdumpvm: page table after sbrk + touch ===\n");
  dumpvm();                    // muestra page table después
  exit(0);
}
```

**Propósito**: Visualizar cómo la page table cambia cuando el proceso pide más memoria.

**¿Por qué "tocar" las páginas?** Porque este xv6 usa **lazy allocation**. Cuando llamas `sbrk(8192)`, el kernel solo aumenta `proc->sz` pero NO asigna páginas. La página se asigna cuando el programa la usa por primera vez (esto genera un page fault que vmfault resuelve transparentemente).

Al hacer `p[0] = 'A'`, forzamos un page fault → vmfault asigna la primera página.
Al hacer `p[4096] = 'B'`, forzamos otro page fault → vmfault asigna la segunda página.

La segunda llamada a `dumpvm()` debería mostrar 2 páginas más que la primera.

### 6.4 tmemro — Página de solo lectura y page fault

```c
int main(void) {
  uint64 va = 0x40000000;     // dirección elegida (1 GB)

  printf("tmemro: mapping read-only page at va=0x%lx\n", va);
  int ret = map_ro((void*)va); // mapea página de solo lectura

  char *p = (char*)va;
  printf("tmemro: reading from RO page: \"%s\"\n", p);  // lee OK

  printf("\ntmemro: now attempting WRITE to read-only page...\n");
  *p = 'X';  // ESTO GENERA UN PAGE FAULT → el kernel mata el proceso

  // Esta línea NUNCA se ejecuta
  printf("tmemro: ERROR - write succeeded\n");
  exit(1);
}
```

**Propósito**: Demostrar protección de memoria — el kernel puede crear páginas que el usuario puede leer pero NO escribir.

**¿Por qué 0x40000000?** Es una dirección arbitraria (1 GB en el espacio de direcciones) que sabemos que no está usada por el programa. Podría ser cualquier dirección libre.

**Flujo esperado**:
1. `map_ro` crea la página con permisos R (lectura) y U (usuario), SIN W (escritura)
2. Leer la página funciona → muestra "Hola desde el kernel xv6 - pagina solo lectura"
3. Intentar escribir (`*p = 'X'`) genera scause=15 (store page fault)
4. El kernel detecta que la página está mapeada pero no es escribible
5. vmfault devuelve 0 (no puede resolver — la página ya existe)
6. trap.c imprime el mensaje de page fault y mata el proceso

**Salida esperada**:
```
tmemro: mapping read-only page at va=0x40000000
tmemro: map_ro returned 0 (success)
tmemro: reading from RO page: "Hola desde el kernel xv6 - pagina solo lectura"

tmemro: now attempting WRITE to read-only page...
tmemro: this should trigger a store page fault (scause=15)
[trap] pid 4: store page fault at va=0x40000000, scause=15 - killing process
```

### 6.5 tuargs — Prueba de robustez

```c
int main(void) {
  // Activa rastreo para write, read, open y hello
  int mask = (1 << SYS_write) | (1 << SYS_read) | (1 << SYS_open) | (1 << SYS_hello);
  trace(mask);

  // Test 1: hello() normal
  hello();

  // Test 2: write con file descriptor inválido
  write(99, "test", 4);       // fd 99 no existe → debe devolver -1

  // Test 3: read con file descriptor inválido
  read(99, buf, sizeof(buf)); // fd 99 no existe → debe devolver -1

  // Test 4: open con puntero inválido
  open((char*)0xdeadbeef, 0); // dirección basura → debe devolver -1

  printf("=== tuargs: all tests completed without kernel panic ===\n");
  exit(0);
}
```

**Propósito**: Verificar que el kernel maneja correctamente argumentos inválidos sin crashear (kernel panic). Un kernel robusto debe devolver errores (-1), NO explotar.

- `write(99, ...)`: el fd 99 no existe, el kernel debe devolver -1
- `read(99, ...)`: ídem
- `open(0xdeadbeef, ...)`: la dirección 0xdeadbeef es basura, `fetchstr` fallará al intentar leer el string y devolverá -1

El trace está activado para que se vean las syscalls con sus argumentos y valores de retorno, mostrando que efectivamente devuelven -1.

---

## 7. Cómo probar todo

### 7.1 Copiar archivos a la VM

Copia los 15 archivos de `D:\Descargas\xv6-proyecto2\` a tu máquina virtual Pop!_OS, reemplazando los archivos existentes en el repositorio xv6.

**Archivos a REEMPLAZAR** (ya existen):
```
kernel/syscall.h
kernel/proc.h
kernel/syscall.c
kernel/sysproc.c
kernel/vm.c
kernel/defs.h
kernel/trap.c
user/user.h
user/usys.pl
Makefile
```

**Archivos NUEVOS** (copiar a user/):
```
user/thello.c
user/ttrace.c
user/tdumpvm.c
user/tmemro.c
user/tuargs.c
```

### 7.2 Compilar y ejecutar

```bash
make clean
make qemu
```

Si todo compila sin errores, verás el prompt de xv6: `$`

### 7.3 Probar cada comando

```bash
$ thello
# Esperado: mensaje del kernel + "returned 42"

$ ttrace 134217728 thello
# Esperado: lo mismo pero con líneas [trace]

$ tdumpvm
# Esperado: dos dumps de page table, el segundo con más páginas

$ tmemro
# Esperado: lee OK, luego page fault al intentar escribir

$ tuargs
# Esperado: todos los tests pasan, sin kernel panic
```

### 7.4 Errores comunes

- **"unknown sys call 27"**: falta agregar `sys_hello` a la tabla `syscalls[]` en syscall.c
- **"undefined reference to vmprint"**: falta la declaración en defs.h
- **Kernel panic en mappages: remap**: la dirección `0x40000000` ya está mapeada (improbable, pero si pasa, cambia la dirección en tmemro.c)
- **thello no aparece en la shell**: falta la entrada `$U/_thello\` en el Makefile
