#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

int main(void)
{
    // Variable para guardar el "file descriptor"
    int fd;
    // Variable que representará UNA entrada del directorio
    struct dirent entrada;

    // Abrimos el directorio actual (".")
    fd = open(".", 0);

    // Verificamos si hubo error
    if (fd < 0) {
        printf("Error: no se pudo abrir el directorio\n");
        exit(1);
    }

    printf("Contenido del directorio actual:\n");

    // Leeremos el directorio entrada por entrada
    while (1) {

        // Intentamos leer una entrada completa
        int bytes_leidos = read(fd, &entrada, sizeof(entrada));

        // Si no se leyó el tamaño completo, ya no hay más archivos
        if (bytes_leidos != sizeof(entrada)) {
            break;
        }

        // Si el número interno es 0, significa que está vacío
        if (entrada.inum == 0) {
           continue;  // saltar a la siguiente vuelta
        }

        // Imprimir el nombre del archivo
        printf("%s\n", entrada.name);
    }

    // Cerramos el directorio
    close(fd);

    // Terminar el programa correctamente
    exit(0);
}
