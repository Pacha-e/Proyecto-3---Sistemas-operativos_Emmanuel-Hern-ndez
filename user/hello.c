#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(void)
{
  printf("hello: calling syscall hello()...\n");
  int ret = hello();
  printf("hello: hello() returned %d\n", ret);
  exit(0);
}
