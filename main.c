#include <memory.h>
#include <stdio.h>
#include <string.h>

int main() {
  memory_manager_init();
  // Abrir el archivo de log
  open_log_file();
  malloc_control(FIRST_FIT);

  // Ejemplo de uso
  char *ptr1 = call_malloc(100);
  int *ptr2 = call_calloc(10, sizeof(int));
  ptr1 = call_realloc(ptr1, 200);
  call_free(ptr1, 1);
  call_free(ptr2, 1);

  memory_manager_cleanup();
  // Cerrar el archivo de log
  close_log_file();

  return 0;
}
