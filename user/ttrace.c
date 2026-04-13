#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 3){
    printf("Usage: ttrace <mask> <command> [args...]\n");
    printf("  mask is a bitmask of syscall numbers to trace\n");
    printf("  Example: ttrace 134217728 thello   (trace hello = 1<<27)\n");
    printf("  Example: ttrace 65536 echo hi       (trace write = 1<<16)\n");
    exit(1);
  }
  int mask = atoi(argv[1]);
  trace(mask);
  exec(argv[2], &argv[2]);
  printf("ttrace: exec %s failed\n", argv[2]);
  exit(1);
}
