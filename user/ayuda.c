#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
    printf("Comandos disponibles:\n");
    printf(" - listar\n");
    printf(" - leer <archivo>\n");
    printf(" - tiempo\n");
    printf(" - calc <num1> <operador> <num2>\n");
    printf(" - ayuda\n");
    printf(" - salir\n");

    exit(0);
}
