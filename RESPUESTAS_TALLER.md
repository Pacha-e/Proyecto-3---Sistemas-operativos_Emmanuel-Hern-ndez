# Respuestas Taller: "El Escudo contra Apagones"

## Fase 1: Teoría - La Anatomía de un "Crash" y el Log

### 1. Atomicidad ante fallos
**Pregunta:** ¿Qué significa exactamente que una operación logre ser "atómica con respecto a las caídas (crashes)" gracias a la capa de registro?

**Respuesta:** La atomicidad ante fallos garantiza que una operación compleja del sistema de archivos (que modifica múltiples bloques como inodos, mapas de bits y bloques de datos) se complete en su totalidad o no se aplique en absoluto tras un reinicio. Sin logging, un corte de energía a mitad de una operación podría dejar el sistema en un estado inconsistente (ej. un bloque marcado como usado en el mapa de bits pero no referenciado por ningún inodo). Con logging, los cambios se escriben primero en un área segura (el Log) y solo se pasan a su ubicación final si la transacción se confirmó completamente.

### 2. El Punto de No Retorno (Commit Point)
**Pregunta:** Expliquen cuál es el instante crítico (commit point) que define si una transacción se descarta o si sus datos son reproducidos y recuperados en el reinicio del equipo.

**Respuesta:** El **punto de no retorno** ocurre durante la ejecución de la función `write_head()` en `kernel/log.c`. Este es el momento exacto en que el bloque de cabecera del log (que contiene el número de bloques modificados y sus destinos) se escribe físicamente en el disco.
- **Antes de `write_head()`:** Si hay un fallo, el log en disco no tiene una cabecera válida para la transacción actual, por lo que el sistema de recuperación la ignora.
- **Después de `write_head()`:** La transacción se considera "completada" para el sistema. Aunque los datos aún no estén en su lugar final (home location), la rutina `recover_from_log()` al reiniciar verá la cabecera y terminará de copiar los datos usando `install_trans()`.

### 3. Group Commit (Compromiso Grupal)
**Pregunta:** ¿Qué ventajas de rendimiento trae esta técnica comparada con escribir la cabecera por cada operación de disco individual?

**Respuesta:** 
- **Eficiencia en E/S:** Escribir en el disco es una de las operaciones más lentas en un sistema operativo. Cada escritura de cabecera es síncrona y obliga al disco a esperar. El "Group Commit" permite que múltiples procesos (que ejecutan llamadas como `write`, `mkdir` o `unlink`) "suban" sus cambios al mismo tren. En lugar de detener el sistema para escribir 10 cabeceras para 10 operaciones, se escribe **una sola vez** la cabecera para las 10, amortizando enormemente el costo de latencia del hardware.
- **Mejor rendimiento bajo carga:** A medida que hay más procesos realizando operaciones de archivos, el beneficio aumenta, ya que se procesan más cambios por cada operación de escritura física del log.

## Fase 2: El Reto de Código - "La Transacción Segura"

### 1. El Inicio (begin_op)
**Pregunta:** ¿Cuál es la constante límite de bloques (MAXOPBLOCKS) y por qué se asume de manera conservadora?

**Respuesta:** La constante `MAXOPBLOCKS` es **10** (definida en `kernel/param.h`). Se asume de manera conservadora porque representa el número máximo de bloques que una sola operación del sistema de archivos (como escribir un fragmento de archivo o crear un directorio) podría necesitar modificar. Dado que el log tiene un tamaño fijo (`LOGBLOCKS = 30`), el sistema debe asegurar que nunca se empiece una operación que pudiera exceder el espacio restante, teniendo en cuenta otras operaciones concurrentes.

### 2. La Modificación (log_write)
**Pregunta:** ¿Por qué es absolutamente vital asegurar que este búfer modificado no sea escrito a su lugar real en el disco antes del commit final?

**Respuesta:** Es vital porque si un bloque modificado se escribiera en su "home location" antes de que toda la transacción esté asegurada en el Log, se rompería la **atomicidad**. Si el sistema fallara en ese instante, el disco tendría una parte de la operación aplicada y otra no, resultando en una corrupción del sistema de archivos. El "pinning" (fijación) del búfer en la caché impide que el manejador de buffers lo expulse y lo escriba en el disco antes de tiempo.

### 3. El Cierre (end_op)
**Pregunta:** Enumeren y expliquen a detalle las cuatro fases que se ejecutan (incluyendo write_log e install_trans) para trasladar los datos desde el Log hacia su estado final en el disco duro.

**Respuesta:** Cuando `end_op` detecta que no hay más operaciones pendientes (`outstanding == 0`), llama a `commit()`, que ejecuta estas cuatro fases:
1.  **`write_log()`**: Copia cada bloque modificado desde la memoria caché (buffer cache) hacia los bloques reservados para el log en el disco.
2.  **`write_head()`**: Escribe el bloque de cabecera del log en el disco. Este es el **punto de commit**. Una vez que esto termina, la transacción es persistente.
3.  **`install_trans()`**: Copia los bloques desde el área del log en el disco hacia sus ubicaciones reales ("home locations") en el sistema de archivos.
4.  **Limpieza del Log**: Se vuelve a llamar a `write_head()` con un conteo de bloques de cero para indicar que el log está vacío y listo para la siguiente transacción.

## Fase 3: Análisis de Diseño y Casos Extremos

### El Dilema de la Absorción (Absorption)
**Pregunta:** ¿Cómo xv6 implementa la optimización llamada "absorción" para manejar estas modificaciones concurrentes al mismo bloque?

**Respuesta:** La absorción se implementa en la función `log_write` de `kernel/log.c`. Antes de añadir un bloque al log, el kernel recorre los bloques ya registrados en la transacción actual (`log.lh.block`). Si encuentra que el número de bloque ya está en la lista, simplemente reutiliza esa entrada en lugar de añadir una nueva. Esto asegura que, por ejemplo, si el bloque del mapa de bits se modifica 5 veces durante una transacción, solo se guarde una copia en el disco al hacer el commit, ahorrando espacio y tiempo de escritura.

### El Dilema del Tamaño del Log
**Pregunta:** ¿Qué ocurre cuando una función nativa como write intenta escribir de golpe un archivo gigantesco que sobrepasa este límite de bloques permitidos? ¿Qué estructura de diseño utiliza xv6 para no violar nunca el espacio predeterminado del log?

**Respuesta:** En xv6, una escritura que exceda el tamaño del log se fragmenta en múltiples transacciones más pequeñas. Esto se gestiona en la función `filewrite` dentro de `kernel/file.c`. El diseño utiliza un bucle `while` que divide la petición original en fragmentos (chunks). Cada fragmento se limita a un tamaño máximo (`max`) calculado para que la operación (incluyendo metadatos como inodos y bloques indirectos) nunca exceda `MAXOPBLOCKS`. Cada fragmento se encierra en su propio par de `begin_op()` y `end_op()`, lo que garantiza que el sistema de archivos siempre permanezca consistente, aunque el archivo se escriba por partes.
