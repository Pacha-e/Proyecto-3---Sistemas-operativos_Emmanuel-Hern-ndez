#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

struct {
  char *name;
  int num;
} syscall_lookup[] = {
  {"fork",    SYS_fork},
  {"exit",    SYS_exit},
  {"wait",    SYS_wait},
  {"pipe",    SYS_pipe},
  {"read",    SYS_read},
  {"kill",    SYS_kill},
  {"exec",    SYS_exec},
  {"fstat",   SYS_fstat},
  {"chdir",   SYS_chdir},
  {"dup",     SYS_dup},
  {"getpid",  SYS_getpid},
  {"sbrk",    SYS_sbrk},
  {"pause",   SYS_pause},
  {"uptime",  SYS_uptime},
  {"open",    SYS_open},
  {"write",   SYS_write},
  {"mknod",   SYS_mknod},
  {"unlink",  SYS_unlink},
  {"link",    SYS_link},
  {"mkdir",   SYS_mkdir},
  {"close",   SYS_close},
  {"hello",   SYS_hello},
  {"trace",   SYS_trace},
  {"dumpvm",  SYS_dumpvm},
  {"map_ro",  SYS_map_ro},
  {0, 0},
};

int
name_to_mask(char *name)
{
  for(int i = 0; syscall_lookup[i].name; i++){
    if(strcmp(name, syscall_lookup[i].name) == 0)
      return 1 << syscall_lookup[i].num;
  }
  return -1;
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    printf("Uso: trace <syscall> <comando> [args...]\n");
    printf("  syscall puede ser un nombre o un numero de mascara\n");
    printf("  Ejemplos:\n");
    printf("    trace hello hello\n");
    printf("    trace write echo hola\n");
    printf("    trace read cat README.md\n");
    exit(1);
  }

  int mask = name_to_mask(argv[1]);
  if(mask < 0){
    mask = atoi(argv[1]);
    if(mask == 0){
      printf("trace: syscall '%s' no reconocida\n", argv[1]);
      exit(1);
    }
  }

  trace(mask);
  exec(argv[2], &argv[2]);
  printf("trace: exec %s failed\n", argv[2]);
  exit(1);
}
