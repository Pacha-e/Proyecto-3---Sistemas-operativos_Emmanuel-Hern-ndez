#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
    if(argc < 2){
        printf("Uso: leer <archivo>\n");
        exit(1);
    }

    int fd = open(argv[1], 0);  // 0 = O_RDONLY (solo lectura)

    if(fd < 0){
        printf("No se pudo abrir el archivo\n");
        exit(1);
    }

    char buf[512];
    int n;

    while((n = read(fd, buf, sizeof(buf))) > 0){
        write(1, buf, n);  // 1 = stdout (pantalla)
    }

    close(fd);
    exit(0);
}
