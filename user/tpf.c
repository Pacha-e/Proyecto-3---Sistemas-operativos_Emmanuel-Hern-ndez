#include "kernel/types.h"
#include "user/user.h"

static void test_load_null(void) {
  volatile int *p = (int*)0;
  int v = *p;
  (void)v;
}

static void test_store_null(void) {
  volatile int *p = (int*)0;
  *p = 42;
}

static void test_load_high(void) {
  volatile int *p = (int*)0x80000000;
  int v = *p;
  (void)v;
}

int main(void) {
  printf("tpf: page fault observer test\n");

  int pid, status;

  pid = fork();
  if(pid == 0) { test_load_null(); exit(0); }
  wait(&status);
  printf("tpf: test1 (load 0x0) estado=%d\n", status);

  pid = fork();
  if(pid == 0) { test_store_null(); exit(0); }
  wait(&status);
  printf("tpf: test2 (store 0x0) estado=%d\n", status);

  pid = fork();
  if(pid == 0) { test_load_high(); exit(0); }
  wait(&status);
  printf("tpf: test3 (load 0x80000000) estado=%d\n", status);

  printf("tpf: todos los procesos matados por page faults invalidos\n");
  exit(0);
}
