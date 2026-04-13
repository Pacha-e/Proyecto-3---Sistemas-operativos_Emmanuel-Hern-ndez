#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
    setshm();

    int pid = fork();
    int tiempo = 5;
    char *mem = (char*) getshm();

    if(mem == 0){
        printf("Error: no hay memoria compartida\n");
        exit(1);
    }

    if(pid > 0){
        // 🔴 PADRE

        mem[0] = 'H';
        mem[1] = 'o';
        mem[2] = 'l';
        mem[3] = 'a';
        mem[4] = '\0';

 	printf("PADRE (%d) addr: %p\n", getpid(), mem);
        printf("PADRE (%d) escribió: %s\n", getpid(), mem);
        // espera a que el hijo termine COMPLETAMENTE
        wait(&tiempo);

        // prints después del wait (sin solaparse)
        printf("PADRE (%d) addr: %p\n", getpid(), mem);
        printf("PADRE (%d) escribió: %s\n", getpid(), mem);

    } else {
        // 🔵 HIJO

        printf("HIJO (%d) addr: %p\n", getpid(), mem);
        printf("HIJO (%d) lee: %s\n", getpid(), mem);
    }

    //  esto se ejecuta en ambos, pero ahora sin conflicto
    if(pid > 0){
        printf("PADRE (%d) final: %s (addr: %p)\n", getpid(), mem, mem);
    }

    exit(0);
}
