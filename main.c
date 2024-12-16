/**
 * @file main.c
 * @brief Programa principal para probar el administrador de memoria.
 *
 * Este archivo contiene el programa principal para probar el administrador de
 * memoria.
 */
#include <memory.h>

/**
 * @brief Función principal.
 *
 * @return int Código de salida.
 */
int main() {
  memory_manager_init(); // Inicializar el administrador de memoria

  open_log_file();           // Abrir el archivo de log
  malloc_control(FIRST_FIT); // Configurar la política de asignación

  // Ejemplo de uso
  char *ptr1 = call_malloc(100);
  check_heap();
  int *ptr2 = call_calloc(10, sizeof(int));
  check_heap();
  ptr1 = call_realloc(ptr1, 200);
  check_heap();
  call_free(ptr1, 1);
  check_heap();
  call_free(ptr2, 1);

  memory_manager_cleanup(); // Limpiar el administrador de memoria
  close_log_file();         // Cerrar el archivo de log
  fflush(stdout);           // Limpiar el buffer de salida
  fflush(stderr);           // Limpiar el buffer de errores
  fclose(stdout);           // Cerrar el archivo de salida
  fclose(stderr);           // Cerrar el archivo de errores
  return 0;
}
