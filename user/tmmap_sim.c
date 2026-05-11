#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  printf("tmmap_sim: mmap simulation via mapzero\n");

  int size = 3 * 4096;
  int addr = mapzero(size);
  if(addr < 0) {
    printf("tmmap_sim: mapzero(%d) fallo\n", size);
    exit(1);
  }
  printf("tmmap_sim: mapzero(%d) = 0x%x\n", size, addr);

  char *p = (char*)(uint64)addr;

  printf("tmmap_sim: leyendo 3 paginas (esperado: 'A' en cada una)...\n");
  for(int i = 0; i < 3; i++) {
    char c = p[i * 4096];
    printf("tmmap_sim: pagina[%d] primer byte = '%c' %s\n",
           i, c, (c == 'A') ? "OK" : "ERROR");
  }

  p[0] = 'Z';
  printf("tmmap_sim: escritura OK, p[0]='%c'\n", p[0]);

  printf("tmmap_sim: test completado\n");
  exit(0);
}
