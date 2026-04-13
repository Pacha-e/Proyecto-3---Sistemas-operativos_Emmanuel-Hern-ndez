#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(void)
{
  printf("thello: calling syscall hello()...\n");
  int ret = hello();
  printf("thello: hello() returned %d\n", ret);
  exit(0);
}
