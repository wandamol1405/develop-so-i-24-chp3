#include <memory.h>
#include <stdio.h>
#include <string.h>

int main() {
  // Abrir el archivo de log
  open_log_file("memory_operations.log");
  malloc_control(FIRST_FIT);

  // Ejemplo de uso
  char *ptr1 = malloc(100);
  int *ptr2 = calloc(10, sizeof(int));
  ptr1 = realloc(ptr1, 200);
  free(ptr1);
  free(ptr2);

  // Cerrar el archivo de log
  close_log_file();

  return 0;
}
