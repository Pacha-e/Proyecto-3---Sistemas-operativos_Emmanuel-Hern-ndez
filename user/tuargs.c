#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(void)
{
  printf("=== tuargs: syscall argument robustness test ===\n\n");

  int mask = (1 << SYS_write) | (1 << SYS_read) | (1 << SYS_open) | (1 << SYS_hello);
  trace(mask);
  printf("tuargs: trace mask set to 0x%x\n\n", mask);

  printf("--- Test 1: hello() ---\n");
  int ret = hello();
  printf("hello returned: %d\n\n", ret);

  printf("--- Test 2: write(99, ...) invalid fd ---\n");
  ret = write(99, "test", 4);
  printf("write(99) returned: %d (expected -1)\n\n", ret);

  printf("--- Test 3: read(99, ...) invalid fd ---\n");
  char buf[16];
  ret = read(99, buf, sizeof(buf));
  printf("read(99) returned: %d (expected -1)\n\n", ret);

  printf("--- Test 4: open(0xdeadbeef, 0) invalid pointer ---\n");
  ret = open((char*)0xdeadbeef, 0);
  printf("open(0xdeadbeef) returned: %d (expected -1)\n\n", ret);

  printf("=== tuargs: all tests completed without kernel panic ===\n");
  exit(0);
}
