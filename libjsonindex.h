#ifndef LIBJSONINDEX_H
#define LIBJSONINDEX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Formato del archivo .jnx
 * ------------------------
 * El indice se guarda como texto plano. Cada linea contiene:
 *
 *   ruta<TAB>inicio<TAB>fin<LF>
 *
 * - ruta: camino absoluto estilo JSON Pointer. La raiz se guarda como "$" y
 *   los hijos como /llave/0/campo.
 * - inicio: byte donde empieza el valor en el JSON original.
 * - fin: byte inmediatamente posterior al final del valor.
 *
 * Ejemplo:
 *   /usuarios/0/nombre    36    41
 *
 * Usar fin exclusivo permite extraer con longitud = fin - inicio.
 */

int jnx_build_index(const char *json_path, const char *index_path);
char *jnx_search(const char *json_path, const char *index_path,
                 const char *expression, int *match_count);
char *jnx_extract_fragment(const char *json_path, long start, long end);
int jnx_default_index_path(const char *json_path, char *buffer,
                           size_t buffer_size);
void jnx_free(void *ptr);
const char *jnx_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
