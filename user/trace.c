#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 3){
    printf("Usage: trace <mask> <command> [args...]\n");
    printf("  mask is a bitmask of syscall numbers to trace\n");
    printf("  Example: trace 134217728 hello   (trace hello = 1<<27)\n");
    printf("  Example: trace 65536 echo hi      (trace write = 1<<16)\n");
    exit(1);
  }
  int mask = atoi(argv[1]);
  trace(mask);
  exec(argv[2], &argv[2]);
  printf("trace: exec %s failed\n", argv[2]);
  exit(1);
}
