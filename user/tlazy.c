#include "kernel/types.h"
#include "user/user.h"

#define N_PAGES 8
#define PGSZ    4096

int main(void) {
  printf("tlazy: lazy allocation test (%d paginas)\n", N_PAGES);

  char *base = sbrk(N_PAGES * PGSZ);
  if(base == (char*)-1) {
    printf("tlazy: sbrk fallo\n");
    exit(1);
  }
  printf("tlazy: sbrk(%d) ok, base=0x%lx (sin mapear fisicamente)\n",
         N_PAGES * PGSZ, (uint64)base);

  printf("tlazy: acceso secuencial (genera %d page faults)...\n", N_PAGES);
  for(int i = 0; i < N_PAGES; i++)
    base[i * PGSZ] = (char)i;
  printf("tlazy: acceso secuencial completo\n");

  printf("tlazy: segundo acceso (sin page faults)...\n");
  int sum = 0;
  for(int i = 0; i < N_PAGES; i++)
    sum += (unsigned char)base[i * PGSZ];
  printf("tlazy: suma=%d (correcto: %d)\n", sum, (N_PAGES*(N_PAGES-1))/2);

  char *base2 = sbrk(N_PAGES * PGSZ);
  printf("tlazy: acceso aleatorio a base2=0x%lx...\n", (uint64)base2);
  for(int i = N_PAGES - 1; i >= 0; i--)
    base2[i * PGSZ] = (char)(i * 2);
  printf("tlazy: acceso aleatorio completo\n");

  printf("tlazy: test completado (ver 'page fault: pid=' en output)\n");
  exit(0);
}
