#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  uint64 va = 0x40000000;

  printf("memro: mapping read-only page at va=0x%lx\n", va);
  int ret = map_ro((void*)va);
  if(ret < 0){
    printf("memro: map_ro failed\n");
    exit(1);
  }
  printf("memro: map_ro returned %d (success)\n", ret);

  char *p = (char*)va;
  printf("memro: reading from RO page: \"%s\"\n", p);

  printf("\nmemro: now attempting WRITE to read-only page...\n");
  printf("memro: this should trigger a store page fault (scause=15)\n");
  *p = 'X';

  printf("memro: ERROR - write succeeded (should have been killed)\n");
  exit(1);
}
