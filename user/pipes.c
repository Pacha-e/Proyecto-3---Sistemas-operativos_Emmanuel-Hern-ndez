// pipes.c - Demostraciones de tuberías (pipes) para xv6-riscv
// Autor: Asistente docente (curso Sistemas Operativos)
// Uso: añadir este archivo a user/ del árbol de xv6 y compilar.
// Compilación (desde raíz de xv6):
//   $ make qemu
// Ejecución dentro de xv6:
//   $ pipes
//
// Este archivo muestra, desde espacio de usuario, el funcionamiento
// de las operaciones de pipe: creación, herencia de descriptores,
// lectura/escritura, bloqueo (vacío/lleno), EOF y uso con dup/exec.
//
// Referencias (código y libro xv6-riscv):
//  - kernel/pipe.c: definición de struct pipe, pipewrite/piperead, pipeclose
//  - kernel/sysfile.c: implementación de sys_pipe y fdalloc/copyout
//  - Libro xv6-riscv, §1.3 Pipes: semántica de read/write/EOF/bloqueo
//
// Nota: A continuación se incluye como COMENTARIO una “instantánea”
//       de fragmentos clave del kernel para uso pedagógico.
//       El código que compila es el de las funciones demo_* y main().
/*
//---------------------------------------
// Referencia (kernel/pipe.c)
//---------------------------------------
#define PIPESIZE 512
struct pipe {
  struct spinlock lock;
  char  data[PIPESIZE];
  uint  nread;
  uint  nwrite;
  int   readopen;
  int   writeopen;
};
// Vacío:  nread == nwrite
// Lleno:  nwrite == nread + PIPESIZE

//int pipewrite(struct pipe *pi, uint64 addr, int n) { }
//int piperead(struct pipe *pi, uint64 addr, int n) { }
//void pipeclose(struct pipe *pi, int writable) { }

//---------------------------------------
// Referencia (kernel/sysfile.c)
//---------------------------------------
uint64 sys_pipe(void) {
  uint64 ufd; int fd0, fd1; struct file *rf, *wf;
  if (argaddr(0, &ufd) < 0) return -1;
  if (pipealloc(&rf, &wf) < 0) return -1;
  fd0 = fdalloc(rf); fd1 = fdalloc(wf);
  return 0;
}
*/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// ----------------------------------------
// Utilidades
// ----------------------------------------

// Escribe exactamente n bytes, reintentando si es necesario.
static int write_exact(int fd, const void *buf, int n) {
  int total = 0;
  const char *p = (const char*)buf;
  while (total < n) {
    int r = write(fd, p + total, n - total);
    if (r < 0) return r;
    total += r;
  }
  return total;
}

// Lee hasta n bytes, retornando la cantidad leída (puede ser 0 por EOF).
static int read_some(int fd, void *buf, int n) {
  return read(fd, buf, n);
}

// Rellena un buffer con un patrón repetitivo legible.
static void fill_pattern(char *buf, int n, char base) {
  for (int i = 0; i < n; i++) buf[i] = (char)(base + (i % 26));
}

// Imprime un separador visual.
static void banner(const char *title) {
  printf("\n================ %s ================\n", title);
}

// ----------------------------------------
// 1) Demostración básica: write/read en el mismo proceso
// ----------------------------------------
static void demo_basico(void) {
  banner("demo_basico: write/read en el mismo proceso");
  int p[2];
  if (pipe(p) < 0) {
    printf("pipe: error\n");
    return;
  }
  const char *msg = "hola xv6\n";
  if (write_exact(p[1], msg, 9) < 0) {
    printf("write: error\n");
  }
  char buf[32] = {0};
  int n = read_some(p[0], buf, sizeof(buf));
  printf("leido(%d): %s", n, buf);
  close(p[0]); close(p[1]);
}

// ----------------------------------------
// 2) Herencia de descriptores con fork: padre escribe, hijo lee
//    y muestra EOF cuando el padre cierra el extremo de escritura.
// ----------------------------------------
static void demo_fork_herencia(void) {
  banner("demo_fork_herencia: padre escribe, hijo lee y ve EOF");
  int p[2];
  if (pipe(p) < 0) { printf("pipe: error\n"); return; }

  int pid = fork();
  if (pid < 0) {
    printf("fork: error\n");
    close(p[0]); close(p[1]);
    return;
  }

  if (pid == 0) {
    // Hijo: cierra el extremo de escritura, lee hasta EOF.
    close(p[1]);
    char buf[64];
    int total = 0;
    int n;
    while ((n = read_some(p[0], buf, sizeof(buf))) > 0) {
      write_exact(1, buf, n);
      total += n;
    }
    printf("(hijo) total_leido=%d; EOF=%s\n", total, n==0?"si":"no");
    close(p[0]);
    exit(0);
  } else {
    // Padre: escribe y luego cierra, provocando EOF en el hijo.
    const char *linea = "linea-1 en pipe\n";
    write_exact(p[1], linea, strlen(linea));
    const char *linea2 = "linea-2 en pipe\n";
    write_exact(p[1], linea2, strlen(linea2));
    close(p[1]);   // <- indica fin de datos para el lector
    wait(0);
    close(p[0]);
  }
}

// ----------------------------------------
// 3) Bloqueo por pipe vacío: read se bloquea hasta que haya datos
// ----------------------------------------
static void demo_bloqueo_vacio(void) {
  banner("demo_bloqueo_vacio: read espera datos (sleep/wakeup)");
  int p[2];
  if (pipe(p) < 0) { printf("pipe: error\n"); return; }

  int pid = fork();
  if (pid < 0) { printf("fork: error\n"); close(p[0]); close(p[1]); return; }

  if (pid == 0) {
    // Hijo lector: se bloquea porque el buffer está vacío.
    close(p[1]);
    char buf[32] = {0};
    int n = read_some(p[0], buf, sizeof(buf));
    printf("(hijo) desbloqueado, leido=%d: %s", n, buf);
    close(p[0]);
    exit(0);
  } else {
    // Padre escritor: espera un poco y escribe.
    pause(20);  // dar tiempo a que el hijo haga read() y se bloquee
    const char *msg = "desbloqueo por datos\n";
    write_exact(p[1], msg, strlen(msg));
    close(p[1]);
    wait(0);
    close(p[0]);
  }
}

// ----------------------------------------
// 4) Bloqueo por pipe lleno: write se bloquea hasta que el lector libere
//    Estrategia: el padre intenta escribir > PIPESIZE; el hijo lee y libera.
// ----------------------------------------
static void demo_bloqueo_lleno(void) {
  banner("demo_bloqueo_lleno: write espera cuando el buffer está lleno");
  int p[2];
  if (pipe(p) < 0) { printf("pipe: error\n"); return; }

  int pid = fork();
  if (pid < 0) { printf("fork: error\n"); close(p[0]); close(p[1]); return; }

  if (pid == 0) {
    // Hijo: lector. Espera un poco para permitir que el padre llene el buffer.
    close(p[1]);
    pause(10);
    char buf[128];
    int total = 0, n;
    while ((n = read_some(p[0], buf, sizeof(buf))) > 0) {
      total += n;
    }
    printf("(hijo) leido_total=%d bytes; fin (EOF)\n", total);
    close(p[0]);
    exit(0);
  } else {
    // Padre: escritor. Intenta escribir mucho más de 512 bytes.
    close(p[0]);
    const int N = 2000;  // > PIPESIZE para forzar bloqueo incremental
    char big[N]; fill_pattern(big, N, 'a');
    int r = write_exact(p[1], big, N);
    printf("(padre) write_exact retorno=%d (parte pudo bloquear hasta que hijo leyera)\n", r);
    close(p[1]);
    wait(0);
  }
}

// ----------------------------------------
// 5) Redirección con dup + exec: ejemplo estilo libro (wc)
//    Conecta el stdin del proceso hijo a la lectura de la pipe y
//    ejecuta 'wc'. El padre escribe por la pipe.
// ----------------------------------------
static void demo_dup_exec_wc(void) {
  banner("demo_dup_exec_wc: redirigir stdin y ejecutar 'wc'");
  int p[2];
  if (pipe(p) < 0) { printf("pipe: error\n"); return; }

  int pid = fork();
  if (pid < 0) { printf("fork: error\n"); close(p[0]); close(p[1]); return; }

  if (pid == 0) {
    // Hijo: conectar p[0] a fd 0 (stdin) y ejecutar wc
    close(0);
    dup(p[0]);         // ahora fd 0 == p[0]
    close(p[0]);
    close(p[1]);
    char *argv[] = { "wc", 0 };
    exec("wc", argv);
    // Si exec falla
    printf("exec wc fallo\n");
    exit(1);
  } else {
    // Padre: escribe y cierra para que wc vea EOF
    close(p[0]);
    const char *data = "hola mundo\nsegunda linea\n";
    write_exact(p[1], data, strlen(data));
    close(p[1]);
    wait(0);
  }
}

// ----------------------------------------
// main(): ejecuta las demostraciones
// ----------------------------------------
int main(int argc, char *argv[]) {
  demo_basico();
  demo_fork_herencia();
  demo_bloqueo_vacio();
  demo_bloqueo_lleno();
  demo_dup_exec_wc();
  printf("\n[OK] Demos de pipes completadas.\n");
  exit(0);
}
