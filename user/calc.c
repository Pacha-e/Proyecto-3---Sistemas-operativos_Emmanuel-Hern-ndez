#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int num1, num2, resultado;

    // Verificar número correcto de argumentos
    if(argc != 4){
        printf("Uso: calc <num1> <operador> <num2>\n");
        exit(1);
    }

    // Convertir argumentos a enteros
    num1 = atoi(argv[1]);
    num2 = atoi(argv[3]);

    // Evaluar operador
    if(strcmp(argv[2], "+") == 0){
        resultado = num1 + num2;
    }
    else if(strcmp(argv[2], "-") == 0){
        resultado = num1 - num2;
    }
    else if(strcmp(argv[2], "*") == 0){
        resultado = num1 * num2;
    }
    else if(strcmp(argv[2], "/") == 0){
        if(num2 == 0){
            printf("Error: division por cero\n");
            exit(1);
        }
        resultado = num1 / num2;
    }
    else{
        printf("Operador no valido\n");
        exit(1);
    }

    printf("Resultado: %d\n", resultado);
    exit(0);
}
