#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/date.h"

int
main(void)
{
    struct rtcdate r;

    // Obtener fecha actual
    date(&r);

    printf("Fecha actual: %d-%d-%d %d:%d:%d\n",
           r.year,
           r.month,
           r.day,
           r.hour,
           r.minute,
           r.second);

    exit(0);
}
