/**
 * @file test_memory.c
 * @brief Pruebas de asignación de memoria con diferentes políticas.
 *
 * Este archivo contiene las pruebas de asignación de memoria, probando la
 * eficiencia de cada politica de asignación.
 */
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

/** Archivo de log para pruebas*/
FILE *log_test_file = NULL;
/** Activador de impresión de las estadisticas de uso de memoria */
#define PRINT_USAGE 0
/** Número de asignaciones a realizar */
#define NUM_ALLOCATIONS 100
/** Tamaño mínimo de un bloque */
#define MIN_SIZE 2
/** Tamaño máximo de un bloque */
#define MAX_SIZE 1028

/**
 * @brief Obtiene el tiempo actual en microsegundos.
 *
 * @return long Tiempo actual en microsegundos.
 */
long get_time_in_microseconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

/**
 * @brief Limpia las asignaciones de memoria.
 *
 * @param allocations Arreglo de punteros a las asignaciones.
 * @param num_allocs Número de asignaciones.
 */
void cleanup_allocations(void **allocations, int num_allocs) {
  for (int i = 0; i < num_allocs; ++i) {
    if (allocations[i] != NULL) {
      // Solo liberar si no ha sido liberado antes
      call_free(allocations[i], 1);
      allocations[i] = NULL; // Marcar como liberado
    }
  }
}

/**
 * @brief Realiza pruebas de asignación de memoria con diferentes políticas.
 *
 * @param policy Política de asignación de memoria.
 */
void test_policies(int policy) {
  srand(time(NULL));

  void *allocations[NUM_ALLOCATIONS];
  memset(allocations, 0, sizeof(allocations)); // Inicializar punteros a NULL

  malloc_control(policy); // Configurar la política de asignación

  long start_time = get_time_in_microseconds();

  for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
    size_t size = (rand() % (MAX_SIZE - MIN_SIZE + 1)) + MIN_SIZE;

    allocations[i] = call_malloc(size);
    if (allocations[i] == NULL) {
      fprintf(log_test_file, "Allocation %d failed. Requested: %zu bytes\n", i,
              size);
      continue;
    }

    // Liberar aleatoriamente un bloque existente
    if (rand() % 2 == 0) {
      int free_index = rand() % (i + 1); // Seleccionar un índice previo
      if (allocations[free_index] != NULL) {
        // Liberar el bloque solo si no ha sido liberado previamente
        if (allocations[free_index] != NULL) {
          call_free(allocations[free_index], 1);
          allocations[free_index] = NULL; // Marcar como liberado
        }
      }
    }
  }

  long end_time = get_time_in_microseconds();

  // Obtener estadísticas de memoria
  MemoryUsage usage = memory_usage(PRINT_USAGE);

  // Liberar todas las asignaciones restantes
  cleanup_allocations(allocations, NUM_ALLOCATIONS);

  double time_taken = (end_time - start_time) / 1e6;

  // Reporte de resultados
  fprintf(log_test_file, "Policy %d:\n", policy);
  fprintf(log_test_file, "  Time taken: %.6f seconds\n", time_taken);
  fprintf(log_test_file, "  Total allocated: %zu bytes\n",
          usage.total_assigned);
  fprintf(log_test_file, "  Total free: %zu bytes\n", usage.total_free);
  fprintf(log_test_file, "  Internal fragmentation: %zu bytes\n",
          usage.internal_fragmentation);
  fprintf(log_test_file, "  External fragmentation: %zu bytes\n",
          usage.external_fragmentation);
  fprintf(log_test_file, "  Total fragmentation: %zu bytes\n\n",
          usage.total_fragmentation);
  fflush(log_test_file);
}

/**
 * @brief Abre el archivo de log para las pruebas.
 */
void open_log_test_file() {
  log_test_file = fopen("test.log", "w");
  if (log_test_file == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Cierra el archivo de log para las pruebas.
 */
void close_log_test_file() {
  if (log_test_file != NULL) {
    fclose(log_test_file);
  }
}

/**
 * @brief Función principal.
 *
 * @return int Código de salida.
 */
int main() {

  memory_manager_init(); // Inicializar el administrador de memoria

  open_log_test_file(); // Abrir el archivo de log para pruebas

  open_log_file(); // Abrir el archivo de log

  fprintf(log_test_file, "Memory allocation policies test\n\n");

  fprintf(log_test_file, "Testing First Fit Policy\n");
  test_policies(FIRST_FIT);

  fprintf(log_test_file, "Testing Best Fit Policy\n");
  test_policies(BEST_FIT);

  fprintf(log_test_file, "Testing Worst Fit Policy\n");
  test_policies(WORST_FIT);

  memory_manager_cleanup(); // Limpiar el administrador de memoria

  close_log_file(); // Cerrar el archivo de log

  close_log_test_file(); // Cerrar el archivo de log para pruebas

  fflush(stdout); // Limpiar el buffer de salida
  fflush(stderr); // Limpiar el buffer de errores
  fclose(stdout); // Cerrar el archivo de salida
  fclose(stderr); // Cerrar el archivo de errores

  return 0;
}
