#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  printf("=== tdumpvm: initial page table ===\n");
  dumpvm();

  printf("\n=== tdumpvm: allocating 8192 bytes with sbrk ===\n");
  char *p = sbrk(8192);
  if(p == (char*)-1){
    printf("tdumpvm: sbrk failed\n");
    exit(1);
  }
  p[0] = 'A';
  p[4096] = 'B';
  printf("Touched pages at %p and %p\n", p, p + 4096);

  printf("\n=== tdumpvm: page table after sbrk + touch ===\n");
  dumpvm();
  exit(0);
}
