#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

FILE *log_test_file = NULL;

long get_time_in_microseconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

void cleanup_allocations(void **allocations, int num_allocs) {
  for (int i = 0; i < num_allocs; ++i) {
    if (allocations[i] != NULL) {
      // Solo liberar si no ha sido liberado antes
      call_free(allocations[i], 1);
      allocations[i] = NULL; // Marcar como liberado
    }
  }
}

void test_policies(int policy) {
  srand(time(NULL));
  const int num_allocs = 100;
  const size_t min_size = 2;
  const size_t max_size = 1028;

  void *allocations[num_allocs];
  memset(allocations, 0, sizeof(allocations)); // Inicializar punteros a NULL

  malloc_control(policy); // Configurar la política de asignación

  long start_time = get_time_in_microseconds();
  size_t total_requested = 0; // Rastrear el tamaño total solicitado

  for (int i = 0; i < num_allocs; ++i) {
    size_t size = (rand() % (max_size - min_size + 1)) + min_size;
    total_requested += size;

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
  MemoryUsage usage = memory_usage(0);

  // Liberar todas las asignaciones restantes
  cleanup_allocations(allocations, num_allocs);

  double time_taken = (end_time - start_time) / 1e6;

  // Reporte de resultados
  fprintf(log_test_file, "Policy %d:\n", policy);
  fprintf(log_test_file, "  Time taken: %.6f seconds\n", time_taken);
  fprintf(log_test_file, "  Total requested: %zu bytes\n", total_requested);
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

void open_log_test_file() {
  log_test_file = fopen("test.log", "w");
  if (log_test_file == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }
}

void close_log_test_file() {
  if (log_test_file != NULL) {
    fclose(log_test_file);
  }
}

int main() {
  memory_manager_init();
  open_log_test_file();
  open_log_file();
  fprintf(log_test_file, "Memory allocation policies test\n\n");

  fprintf(log_test_file, "Testing First Fit Policy\n");
  test_policies(FIRST_FIT);

  fprintf(log_test_file, "Testing Best Fit Policy\n");
  test_policies(BEST_FIT);

  fprintf(log_test_file, "Testing Worst Fit Policy\n");
  test_policies(WORST_FIT);

  memory_manager_cleanup();
  close_log_file();
  close_log_test_file();
  fflush(stdout);
  fflush(stderr);
  fclose(stdout);
  fclose(stderr);

  return 0;
}
