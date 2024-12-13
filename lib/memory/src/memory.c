#include "memory.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

typedef struct s_block *t_block;
typedef struct memory_stats memory_stats;
void *base = NULL;
int method = 0;
FILE *log_file = NULL;

void open_log_file() {
  log_file = fopen(FILENAME_LOG, "w");
  if (log_file == NULL) {
    perror("No se pudo abrir el archivo de log");
    exit(1);
  }
}

// Función para registrar las operaciones de memoria
void log_memory_operation(const char *operation, void *ptr, size_t size) {
  if (log_file != NULL) {
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(log_file, "[%s] Operation: %s, Address: %p, Size: %zu bytes\n",
            time_str, operation, ptr, size);
    fflush(log_file); // Asegurar que se escriba inmediatamente en el archivo
  }
}

t_block find_block(t_block *last, size_t size) {
  if (method != FIRST_FIT && method != BEST_FIT && method != WORST_FIT) {
    printf("Error: invalid method\n");
    return NULL;
  }

  t_block b = base;
  t_block result = NULL; // Resultado final
  size_t diff =
      (method == BEST_FIT) ? PAGESIZE : (size_t)-1; // Inicialización específica

  while (b) {
    if (b->free) {
      if (method == FIRST_FIT) {
        if (b->size >= size) {
          return b;
        }
      } else if (method == BEST_FIT) {
        if (b->size >= size && (b->size - size) < diff) {
          diff = b->size - size;
          result = b;
        }
      } else if (method == WORST_FIT) {
        if (b->size >= size && (b->size - size) > diff) {
          diff = b->size - size;
          result = b;
        }
      }
    }
    *last = b; // Actualizar el último bloque visitado
    b = b->next;
  }
  return (method == FIRST_FIT)
             ? NULL
             : result; // Para First Fit, NULL si no se encuentra
}

void split_block(t_block b, size_t s) {
  if (b->size <= s + BLOCK_SIZE) {
    return;
  }
  // Comprobación de seguridad para asegurarse de que 'b->data + s' está dentro
  // de los límites del bloque
  if ((b->data + s) >= (b->data + b->size)) {
    printf("Error: 'new' apunta fuera del bloque original\n");
    return;
  }

  t_block new;
  new = (t_block)(b->data + s);
  new->size = b->size - s - BLOCK_SIZE;
  new->next = b->next;
  new->prev = b;
  new->free = 1;
  new->ptr = new->data; // Set ptr to data
  b->size = s;
  b->next = new;
  if (new->next) {
    new->next->prev = new;
  }
}

void copy_block(t_block src, t_block dst) {
  int *sdata, *ddata;
  size_t i;
  sdata = src->ptr;
  ddata = dst->ptr;

  for (i = 0; (i * 4) < src->size && (i * 4) < dst->size; i++)
    ddata[i] = sdata[i];
}

t_block get_block(void *p) {
  char *tmp;
  tmp = (char *)p;

  tmp -= BLOCK_SIZE;
  return (t_block)tmp;
}

int valid_addr(void *p) {
  t_block b = base;
  while (b) {
    if ((char *)p > (char *)b && (char *)p < (char *)(b->data + b->size)) {
      return p == (get_block(p))->ptr;
    }
    b = b->next;
  }
  return 0; // Dirección inválida
}

t_block fusion(t_block b) {
  // Verifica si el bloque actual está libre
  if (!b->free) {
    return b; // Si el bloque no está libre, no hace nada
  }

  // Verifica si el bloque siguiente existe y está libre
  if (b->next && b->next->free) {
    // Sumar el tamaño del bloque siguiente al bloque actual
    b->size += b->next->size;

    // Saltar el bloque siguiente, conectando al subsiguiente
    b->next = b->next->next;

    // Actualizar el puntero 'prev' del nuevo bloque siguiente
    if (b->next) {
      b->next->prev = b;
    }
  }

  // Retornar el bloque fusionado
  return b;
}

t_block extend_heap(t_block last, size_t s) {
  t_block b;
  b = mmap(0, s, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (b == MAP_FAILED) {
    return NULL;
  }
  b->size = s;
  b->next = NULL;
  b->prev = last;
  b->ptr = b->data;

  if (last)
    last->next = b;

  b->free = 0;
  return b;
}

void get_method(int m) { method = m; }

void set_method(int m) { method = m; }

void malloc_control(int m) {
  if (m == 0) {
    set_method(0);
  } else if (m == 1) {
    set_method(1);
  } else if (m = 2) {
    set_method(2);
  } else {
    printf("Error: invalid method\n");
  }
}

void *my_malloc(size_t size) {
  t_block b, last;
  size_t s;
  s = align(size);

  if (base) {
    last = base;
    b = find_block(&last, s);
    if (b) {
      if ((b->size - s) >= (BLOCK_SIZE + 4))
        split_block(b, s);
      b->free = 0;
    } else {
      b = extend_heap(last, s);
      if (!b)
        return (NULL);
    }
  } else {
    b = extend_heap(NULL, s);
    if (!b)
      return (NULL);
    base = b;
  }
  return (b->data);
}

void my_free(void *ptr) {
  t_block b;

  if (valid_addr(ptr)) { // Verifica si la dirección es válida
    b = get_block(ptr);  // Obtiene el bloque de memoria asociado a 'ptr'
    b->free = 1;         // Marca el bloque como libre

    // Intentar fusionar con el siguiente bloque si está libre
    if (b->next && b->next->free) {
      b = fusion(b); // Fusiona con el siguiente bloque si está libre
    }

    // Intentar fusionar con el bloque previo si está libre
    if (b->prev && b->prev->free) {
      b = fusion(b->prev); // Fusiona con el bloque anterior si está libre
    }

    // Actualizar los punteros de la lista de bloques
    if (b->prev) {
      b->prev->next = b; // Actualiza el puntero 'next' del bloque previo
    } else {
      base = b; // Si es el primer bloque, actualiza la base
    }

    if (b->next) {
      b->next->prev = b; // Actualiza el puntero 'prev' del bloque siguiente
    }
  }
}

void *my_calloc(size_t number, size_t size) {
  size_t *new;
  size_t s4, i;

  if (!number || !size) {
    return (NULL);
  }
  new = my_malloc(number * size);
  if (new) {
    s4 = align(number * size) << 2;
    for (i = 0; i < s4; i++)
      new[i] = 0;
  }

  return (new);
}

void *my_realloc(void *ptr, size_t size) {
  size_t s;
  t_block b, new;
  void *newp;

  if (!ptr)
    ("realloc", ptr, size);
  return (my_malloc(size));

  if (valid_addr(ptr)) {
    s = align(size);
    b = get_block(ptr);

    if (b->size >= s) {
      if (b->size - s >= (BLOCK_SIZE + 4))
        split_block(b, s);
    } else {
      if (b->next && b->next->free &&
          (b->size + BLOCK_SIZE + b->next->size) >= s) {
        fusion(b);
        if (b->size - s >= (BLOCK_SIZE + 4))
          split_block(b, s);
      } else {
        newp = my_malloc(s);
        if (!newp)
          return (NULL);
        new = get_block(newp);
        copy_block(b, new);
        my_free(ptr);
        ("realloc", new, size);
        return (newp);
      }
    }
    ("realloc", ptr, size);
    return (ptr);
  }
  printf("No valid address\n");
  return (NULL);
}

void check_heap(void *data) {
  if (data == NULL) {
    printf("Data is NULL\n");
    return;
  }

  t_block block = get_block(data);

  if (block == NULL) {
    printf("Block is NULL\n");
    return;
  }

  printf("\033[1;33mHeap check\033[0m\n");
  printf("Size: %zu\n", block->size);

  // Verificar consistencia de los punteros
  if (block->next != NULL) {
    printf("Next block: %p\n", (void *)(block->next));
    if (block->next->prev != block) {
      printf("\033[1;31mError: Inconsistent next->prev pointer!\033[0m\n");
    }
  } else {
    printf("Next block: NULL\n");
  }

  if (block->prev != NULL) {
    printf("Prev block: %p\n", (void *)(block->prev));
    if (block->prev->next != block) {
      printf("\033[1;31mError: Inconsistent prev->next pointer!\033[0m\n");
    }
  } else {
    printf("Prev block: NULL\n");
  }

  printf("Free: %d\n", block->free);

  // Verificar dirección y tamaño de los datos
  if (block->ptr != NULL) {
    printf("Beginning data address: %p\n", block->ptr);
    printf("Last data address: %p\n",
           (void *)((char *)(block->ptr) + block->size));
    if (block->size == 0 ||
        block->size > 1e6) { // Ajusta el límite máximo según sea necesario
      printf("\033[1;31mError: Invalid block size (%zu)!\033[0m\n",
             block->size);
    }
  } else {
    printf("Data address: NULL\n");
  }

  // Verificar bloques libres adyacentes no fusionados
  if (block->free && block->next && block->next->free) {
    printf("\033[1;31mWarning: Adjacent free blocks not fused!\033[0m\n");
  }

  // Verificar si el bloque está dentro del rango del heap
  void *heap_start = sbrk(0); // Obtener el inicio del heap
  if ((void *)block < heap_start) {
    printf("\033[1;31mError: Block pointer is out of heap range!\033[0m\n");
  }

  printf("Heap address: %p\n", sbrk(0));
}

memory_stats memory_usage(int active_print) {
  t_block current = base;
  size_t total_assigned = 0;
  size_t total_free = 0;
  size_t internal_fragmentation = 0;
  size_t external_fragmentation = 0;

  while (current != NULL) {
    if (current->free) {
      total_free += current->size;

      // Considerar como fragmentación externa solo bloques inutilizables
      if (current->size < BLOCK_SIZE) {
        external_fragmentation += current->size;
      }
    } else {
      total_assigned += current->size;

      // Fragmentación interna: diferencia entre tamaño real y útil
      if (current->size % BLOCK_SIZE != 0) {
        internal_fragmentation += BLOCK_SIZE - (current->size % BLOCK_SIZE);
      }
    }
    current = current->next;
  }

  size_t total_fragmentation = internal_fragmentation + external_fragmentation;

  // Imprimir los resultados
  if (active_print) {
    printf("\033[1;33mMemory usage\033[0m\n");
    printf("Total memory assigned: %zu bytes\n", total_assigned);
    printf("Total free memory: %zu bytes\n", total_free);
    printf("Internal fragmentation: %zu bytes\n", internal_fragmentation);
    printf("External fragmentation: %zu bytes\n", external_fragmentation);
    printf("Total fragmentation: %zu bytes\n", total_fragmentation);
  }
  // Devolver estadísticas en una estructura
  return (memory_stats){total_assigned, total_free, internal_fragmentation,
                        external_fragmentation, total_fragmentation};
}

void *call_malloc(size_t size) {
  void *ptr = my_malloc(size);
  if (ptr)
    log_memory_operation("malloc", ptr, size);
  return ptr;
}

void *call_calloc(size_t num, size_t size) {
  void *ptr = my_calloc(num, size);
  if (ptr)
    log_memory_operation("calloc", ptr, num * size);
  return ptr;
}

void call_free(void *ptr) {
  my_free(ptr);
  log_memory_operation("free", ptr,
                       0); // El tamaño no es necesario para free
}

void *call_realloc(void *ptr, size_t size) {
  void *new_ptr = my_realloc(ptr, size);
  if (new_ptr)
    log_memory_operation("realloc", new_ptr, size);
  return new_ptr;
}

// Función para cerrar el archivo de log
void close_log_file() {
  if (log_file != NULL) {
    fclose(log_file);
  }
}
