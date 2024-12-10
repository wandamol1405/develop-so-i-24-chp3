#include <malloc.h>
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

void test_policies(int policy) {
  srand(time(NULL));
  const int num_allocs = 1000;
  const size_t min_size = 16;
  const size_t max_size = 256;
  size_t ordered_size = 0;

  void *allocations[num_allocs];
  for (int i = 0; i < num_allocs; ++i) {
    allocations[i] = NULL;
  }

  malloc_control(policy);

  long start_time = get_time_in_microseconds();

  // Fase de asignación con liberaciones intercaladas
  for (int i = 0; i < num_allocs; ++i) {
    size_t size = (rand() % (max_size - min_size + 1)) + min_size;
    allocations[i] = malloc(size);
    ordered_size += size;

    if (allocations[i] == NULL) {
      fprintf(log_test_file, "Allocation %d failed.\n", i);
      continue;
    }

    // Liberar aleatoriamente un bloque existente
    if (rand() % 3 == 0) { // Aproximadamente 1 de cada 3 asignaciones se libera
      int free_index = rand() % (i + 1); // Escoger una asignación previa
      if (allocations[free_index] != NULL) {
        free(allocations[free_index]);
        allocations[free_index] = NULL;
      }
    }
  }

  long end_time = get_time_in_microseconds();

  // Estadísticas de uso de memoria
  size_t total_allocated = memory_usage().total_assigned;
  size_t internal_fragmentation = memory_usage().internal_fragmentation;
  size_t external_fragmentation = memory_usage().external_fragmentation;
  size_t total_fragmentation = memory_usage().total_fragmentation;

  // Liberar cualquier asignación restante
  for (int i = 0; i < num_allocs; ++i) {
    if (allocations[i] != NULL) {
      free(allocations[i]);
    }
  }

  double time_taken = (end_time - start_time) / 1e6;

  // Reporte
  fprintf(log_test_file, "Policy %d:\n", policy);
  fprintf(log_test_file, "  Time taken: %.6f seconds\n", time_taken);
  fprintf(log_test_file, "  Total allocated: %zu bytes\n", total_allocated);
  fprintf(log_test_file, "  Internal fragmentation: %zu bytes\n",
          internal_fragmentation);
  fprintf(log_test_file, "  External fragmentation: %zu bytes\n",
          external_fragmentation);
  fprintf(log_test_file, "  Total fragmentation: %zu bytes\n\n",
          total_fragmentation);
  fflush(log_test_file);
}

void open_log_test_file() {
  log_test_file = fopen("test.log", "w");
  if (log_test_file == NULL) {
    perror("Failed to open log file");
    exit(1);
  }
}

int main() {
  open_log_file();
  open_log_test_file();
  fprintf(log_test_file, "Memory allocation policies test\n\n");
  fprintf(log_test_file, "Test First Fit\n");
  test_policies(FIRST_FIT);
  fprintf(log_test_file, "Test Best Fit\n");
  test_policies(BEST_FIT);
  fprintf(log_test_file, "Test Worst Fit\n");
  test_policies(WORST_FIT);
  fclose(log_test_file);
  close_log_file();
  return 0;
}
