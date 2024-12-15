#include <memory.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

typedef struct s_block *t_block;
typedef struct MemoryUsage MemoryUsage;

void *base = NULL;                       // Puntero al primer bloque
int method = FIRST_FIT;                  // Método de asignación de memoria
FILE *log_file = NULL;                   // Archivo de log
size_t count_total_allocated = 0;        // Contador de memoria asignada
size_t count_total_freed = 0;            // Contador de memoria liberada
size_t count_internal_fragmentation = 0; // Contador de fragmentación interna
pthread_mutex_t allocator_lock =
    PTHREAD_MUTEX_INITIALIZER; // Mutex para el allocator

void open_log_file() {
  log_file = fopen(FILENAME_LOG, "w");
  if (log_file == NULL) {
    perror("No se pudo abrir el archivo de log");
  }
}

// Función para registrar las operaciones de memoria
void log_memory_operation(const char *operation, void *ptr, size_t size) {
  if (log_file == NULL) {
    fprintf(stderr, "Error: log_file is NULL. Cannot log operation: %s\n",
            operation);
    return;
  }

  if (ptr == NULL) {
    fprintf(
        stderr,
        "Warning: NULL pointer passed to log_memory_operation. Operation: %s\n",
        operation);
  }

  time_t now = time(NULL);
  struct tm *time_info = localtime(&now);
  char time_str[TIME_STR_SIZE]; // Formato: "YYYY-MM-DD HH:MM:SS"
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

  fprintf(log_file, "[%s] Operation: %s, Address: %p, Size: %zu bytes\n",
          time_str, operation, ptr, size);
  fflush(log_file);
}

t_block find_block(t_block *last, size_t size) {
  t_block b = base;
  t_block selected = NULL;

  // Validación del método
  if (method != FIRST_FIT && method != BEST_FIT && method != WORST_FIT) {
    fprintf(stderr, "Error: Invalid method value %d\n", method);
    return NULL;
  }

  // Selección del método
  if (method == FIRST_FIT) {
    // FIRST_FIT: detenerse en el primer bloque válido
    while (b) {
      if (b->free && b->size >= size) {
        count_internal_fragmentation += b->size - size;
        return b; // Devuelve el primer bloque encontrado
      }
      *last = b;
      b = b->next;
    }
  } else if (method == BEST_FIT) {
    // BEST_FIT: encontrar el bloque con el tamaño más cercano
    size_t min_diff = (size_t)-1;
    while (b) {
      if (b->free && b->size >= size) {
        size_t diff = b->size - size;
        if (diff < min_diff) {
          min_diff = diff;
          selected = b;
          if (diff == 0)
            break; // Si el ajuste es perfecto, detener
        }
      }
      *last = b;
      b = b->next;
    }
    if (selected) {
      count_internal_fragmentation += selected->size - size;
    }
  } else if (method == WORST_FIT) {
    // WORST_FIT: encontrar el bloque más grande que cumpla
    size_t max_size = 0;
    while (b) {
      if (b->free && b->size >= size) {
        if (b->size > max_size) {
          max_size = b->size;
          selected = b;
        }
      }
      *last = b;
      b = b->next;
    }
    if (selected) {
      count_internal_fragmentation += selected->size - size;
    }
  }

  return selected;
}

void split_block(t_block b, size_t s) {
  if (b->size <= s + BLOCK_SIZE) {
    return;
  }
  t_block new;
  new = (t_block)(b->data + s);
  new->size = b->size - s - BLOCK_SIZE;
  new->next = b->next;
  new->prev = b;
  new->free = 1;
  new->ptr = new->data;
  new->is_mapped = 0;
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

  for (i = 0; i * sizeof(size_t) < src->size && i * sizeof(size_t) < dst->size;
       i++)
    ddata[i] = sdata[i];
}

t_block get_block(void *p) {
  if (p == NULL) {
    return NULL;
  }
  return (t_block)((char *)p - offsetof(struct s_block, data));
}

int valid_addr(void *p) {
  if (p == NULL || base == NULL) {
    return INVALID_ADDR;
  }
  t_block b = get_block(p);
  t_block current = base;
  while (current) {
    if (current == b) {
      return (current->ptr == p);
    }
    current = current->next;
  }
  return INVALID_ADDR;
}

t_block fusion(t_block b) {
  // Fusión con los bloques siguientes
  while (b->next && b->next->free) {
    t_block next_block = b->next;
    b->size += BLOCK_SIZE + next_block->size;
    b->next = next_block->next;
    if (b->next)
      b->next->prev = b;
    // Revisar si el bloque fusionado sigue en el espacio mapeado
    if (!next_block->is_mapped) {
      b->is_mapped = 0;
    }
  }

  // Fusión con los bloques previos
  while (b->prev && b->prev->free) {
    t_block prev_block = b->prev;
    prev_block->size += BLOCK_SIZE + b->size;
    prev_block->next = b->next;
    if (b->next)
      b->next->prev = prev_block;
    // Revisar si el bloque fusionado sigue en el espacio mapeado
    if (!b->is_mapped) {
      prev_block->is_mapped = 0;
    }
    b = prev_block;
  }
  return b;
}

t_block extend_heap(t_block last, size_t s) {
  t_block b;
  b = mmap(0, s + BLOCK_SIZE, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (b == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }
  b->size = s;
  b->next = NULL;
  b->prev = last;
  b->ptr = b->data;
  b->free = 0;
  b->is_mapped = 1;

  if (last)
    last->next = b;
  return b;
}

int get_method() { return method; }

void set_method(int m) { method = m; }

void malloc_control(int m) {
  if (m == FIRST_FIT) {
    set_method(FIRST_FIT);
  } else if (m == BEST_FIT) {
    set_method(BEST_FIT);
  } else if (m == WORST_FIT) {
    set_method(WORST_FIT);
  } else {
    fprintf(stderr, "Error: Invalid method value %d\n", m);
  }
}

void *my_malloc(size_t size) {
  pthread_mutex_lock(&allocator_lock);
  t_block b, last;
  size_t s;
  s = align(size);

  if (base) {
    last = base;
    b = find_block(&last, s);
    if (b) {
      if ((b->size - s) >= (BLOCK_SIZE + MIN_BLOCK_DATA_SIZE)) {
        split_block(b, s);
      }
      b->free = 0;
    } else {
      b = extend_heap(last, s);
      if (!b) {
        pthread_mutex_unlock(&allocator_lock);
        return (NULL);
      }
    }
  } else {
    b = extend_heap(NULL, s);
    if (!b) {
      pthread_mutex_unlock(&allocator_lock);
      return (NULL);
    }
    base = b;
  }
  pthread_mutex_unlock(&allocator_lock);
  count_total_allocated += b->size;
  return (b->data);
}

void my_free(void *ptr, int activate_mumap) {
  pthread_mutex_lock(&allocator_lock);
  if (ptr == NULL) {
    pthread_mutex_unlock(&allocator_lock);
    return; // No hay nada que liberar
  }

  t_block b;

  if (valid_addr(ptr)) {
    b = get_block(ptr);

    if (b->free) { // Evitar liberar bloques ya liberados
      fprintf(stderr, "Error: Attempt to free an already freed block.\n");
      pthread_mutex_unlock(&allocator_lock);
      return;
    }

    b->free = 1; // Marcar como libre
    // Intentar fusionar con el siguiente bloque
    b = fusion(b);
    count_total_freed += b->size;
    // Si munmap está habilitado y el bloque es el último
    if (activate_mumap && b->next == NULL) {
      if (b->prev) {
        b->prev->next = NULL;
      } else {
        base = NULL;
      }
      if (b->is_mapped && b->free) {
        size_t total_size = b->size + BLOCK_SIZE;

        if (munmap(b, total_size) == -1) {
          fprintf(stderr, "\033[1;31mError: munmap failed\033[0m\n");
          fprintf(stderr,
                  "\033[1;31mInvalid arguments: b = %p, size = %zu\033[0m\n",
                  (void *)b, total_size);
        } else {
          if (b == base) {
            base = NULL; // Solo actualizar si munmap es exitoso y b es el
                         // primer bloque
          }
        }
      }
    }
  }
  pthread_mutex_unlock(&allocator_lock);
}

void *my_calloc(size_t number, size_t size) {
  pthread_mutex_lock(&allocator_lock);
  size_t *new;
  size_t s4, i;

  if (!number || !size) {
    pthread_mutex_unlock(&allocator_lock);
    return (NULL);
  }
  new = my_malloc(number * size);
  if (new) {
    s4 = align(number * size) << 2;
    for (i = 0; i < s4; i++)
      new[i] = 0;
  }
  pthread_mutex_unlock(&allocator_lock);
  return (new);
}

void *my_realloc(void *ptr, size_t size) {
  pthread_mutex_lock(&allocator_lock);
  size_t s;
  t_block b, new;
  void *newp;

  if (!ptr) {
    pthread_mutex_unlock(&allocator_lock);
    return my_malloc(size);
  }

  if (valid_addr(ptr)) {
    s = align(size);
    b = get_block(ptr);

    if (b->size >= s) {
      if (b->size - s >= (BLOCK_SIZE + MIN_BLOCK_DATA_SIZE))
        split_block(b, s);
    } else {
      if (b->next && b->next->free &&
          (b->size + BLOCK_SIZE + b->next->size) >= s) {
        fusion(b);
        if (b->size - s >= (BLOCK_SIZE + MIN_BLOCK_DATA_SIZE))
          split_block(b, s);
      } else {
        newp = my_malloc(s);
        if (!newp) {
          pthread_mutex_unlock(&allocator_lock);
          return NULL;
        }
        new = get_block(newp);
        if (new->size >= b->size) {
          copy_block(b, new);
          my_free(ptr, 0);
          new->is_mapped = b->is_mapped; // Copy the is_mapped status
          pthread_mutex_unlock(&allocator_lock);
          return newp;
        } else {
          pthread_mutex_unlock(&allocator_lock);
          return NULL;
        }
      }
    }
    pthread_mutex_unlock(&allocator_lock);
    return ptr;
  }
  printf("No valid address\n");
  pthread_mutex_unlock(&allocator_lock);
  return NULL;
}

void check_heap(void) {
  printf("\033[1;33mHeap check\033[0m\n");

  t_block current = base;
  while (current != NULL) {
    printf("Block at %p\n", (void *)current);
    printf("  Size: %zu\n", current->size);
    printf("  Free: %d\n", current->free);

    if (current->next != NULL) {
      printf("  Next block: %p\n", (void *)(current->next));
      if (current->next->prev != current) {
        printf("\033[1;31m  Error: Inconsistent next->prev pointer!\033[0m\n");
      }
    } else {
      printf("  Next block: NULL\n");
    }

    if (current->prev != NULL) {
      printf("  Prev block: %p\n", (void *)(current->prev));
      if (current->prev->next != current) {
        printf("\033[1;31m  Error: Inconsistent prev->next pointer!\033[0m\n");
      }
    } else {
      printf("  Prev block: NULL\n");
    }

    if (current->ptr != NULL) {
      printf("  Beginning data address: %p\n", current->ptr);
      printf("  Last data address: %p\n",
             (void *)((char *)(current->ptr) + current->size));
      if (current->size == 0 || current->size > 1e6) {
        printf("\033[1;31m  Error: Invalid block size (%zu)!\033[0m\n",
               current->size);
      }
    } else {
      printf("  Data address: NULL\n");
    }

    if (current->free && current->next && current->next->free) {
      printf("\033[1;31m  Warning: Adjacent free blocks not fused!\033[0m\n");
    }

    void *heap_start = sbrk(0);
    if ((void *)current < heap_start) {
      printf("\033[1;31m  Error: Block pointer is out of heap range!\033[0m\n");
    }

    current = current->next;
  }

  printf("Heap address: %p\n", sbrk(0));
}

MemoryUsage memory_usage(int active_print) {
  t_block current = base;
  size_t assigned_memory = count_total_allocated;
  count_total_allocated = 0;
  size_t freed_memory = count_total_freed;
  count_total_freed = 0;
  size_t internal_fragmentation = count_internal_fragmentation;
  count_internal_fragmentation = 0;
  size_t external_fragmentation = 0;

  while (current != NULL) {
    if (current->size < (BLOCK_SIZE + MIN_BLOCK_DATA_SIZE)) {
      external_fragmentation += current->size;
    }
    current = current->next;
  }

  size_t total_fragmentation = internal_fragmentation + external_fragmentation;

  // Imprimir los resultados
  if (active_print) {
    printf("\033[1;33mMemory usage\033[0m\n");
    printf("Total memory assigned: %zu bytes\n", assigned_memory);
    printf("Total free memory: %zu bytes\n", freed_memory);
    printf("Internal fragmentation: %zu bytes\n", internal_fragmentation);
    printf("External fragmentation: %zu bytes\n", external_fragmentation);
    printf("Total fragmentation: %zu bytes\n", total_fragmentation);
  }
  // Devolver estadísticas en una estructura
  return (MemoryUsage){assigned_memory, freed_memory, internal_fragmentation,
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

void call_free(void *ptr, int activate_mumap) {
  my_free(ptr, activate_mumap);
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

void memory_manager_init() {
  pthread_mutexattr_t attr;      // Atributos del mutex
  pthread_mutexattr_init(&attr); // Inicializar los atributos
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); // Tipo recursivo
  pthread_mutex_init(&allocator_lock, &attr); // Inicializar el mutex
  pthread_mutexattr_destroy(&attr);           // Destruir los atributos
}

void memory_manager_cleanup() {
  pthread_mutex_destroy(&allocator_lock);
} // Destruir el mutex
