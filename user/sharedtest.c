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
        // PADRE: escribe en memoria compartida
        mem[0] = 'H';
        mem[1] = 'o';
        mem[2] = 'l';
        mem[3] = 'a';
        mem[4] = '\0';

        printf("PADRE (%d) addr: %p\n", getpid(), mem);
        printf("PADRE (%d) escribio: %s\n", getpid(), mem);

        // espera a que el hijo termine
        wait(&tiempo);

        // despues del wait, la pagina sigue viva (refcount > 0)
        printf("PADRE (%d) despues de wait: %s\n", getpid(), mem);

    } else {
        // HIJO: esperar a que el padre escriba
        pause(2);

        printf("HIJO (%d) addr: %p\n", getpid(), mem);
        printf("HIJO (%d) lee: %s\n", getpid(), mem);
    }

    exit(0);
}
