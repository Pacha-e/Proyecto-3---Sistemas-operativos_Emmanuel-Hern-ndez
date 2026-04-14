#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
    printf("Comandos disponibles:\n\n");

    printf("--- Proyecto 1: Comandos basicos ---\n");
    printf("  listar              Lista archivos del directorio\n");
    printf("  leer <archivo>      Muestra el contenido de un archivo\n");
    printf("  tiempo              Muestra el uptime del sistema\n");
    printf("  calc <n1> <op> <n2> Calculadora (+, -, *, /)\n");
    printf("  salir               Termina la shell\n\n");

    printf("--- Proyecto 2: Gestion de memoria ---\n");
    printf("  hello               Ejecuta syscall hello (retorna 42)\n");
    printf("  trace <sc> <cmd>    Traza una syscall al ejecutar un comando\n");
    printf("     Ejemplos: trace hello hello\n");
    printf("               trace write echo hola\n");
    printf("               trace read cat README.md\n");
    printf("  dumpvm              Muestra la tabla de paginas del proceso\n");
    printf("  memro               Mapea pagina read-only y provoca page fault\n");
    printf("  uargs               Prueba robustez de argumentos invalidos\n");
    printf("  sharedtest          Prueba memoria compartida padre/hijo\n\n");

    printf("  ayuda               Muestra este mensaje\n");

    exit(0);
}
