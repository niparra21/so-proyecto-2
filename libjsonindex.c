#include "libjsonindex.h"

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *path;
    long start;
    long end;
} JnxEntry;

typedef struct {
    JnxEntry *items;
    size_t count;
    size_t capacity;
} JnxEntries;

typedef struct {
    char *text;
    size_t length;
    size_t pos;
    JnxEntries entries;
} Parser;

static char g_error[256] = "";

static void set_error(const char *message)
{
    snprintf(g_error, sizeof(g_error), "%s", message);
}

const char *jnx_last_error(void)
{
    return g_error;
}

void jnx_free(void *ptr)
{
    free(ptr);
}

static char *jnx_strdup(const char *text)
{
    size_t len = strlen(text);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static void entries_free(JnxEntries *entries)
{
    size_t i;
    for (i = 0; i < entries->count; i++) {
        free(entries->items[i].path);
    }
    free(entries->items);
    entries->items = NULL;
    entries->count = 0;
    entries->capacity = 0;
}

static int entries_add(JnxEntries *entries, const char *path,
                       long start, long end)
{
    JnxEntry *new_items;
    size_t new_capacity;
    char *path_copy;

    if (entries->count == entries->capacity) {
        new_capacity = entries->capacity == 0 ? 32 : entries->capacity * 2;
        new_items = (JnxEntry *)realloc(entries->items,
                                        new_capacity * sizeof(JnxEntry));
        if (new_items == NULL) {
            set_error("No se pudo reservar memoria para el indice");
            return -1;
        }
        entries->items = new_items;
        entries->capacity = new_capacity;
    }

    path_copy = jnx_strdup(path);
    if (path_copy == NULL) {
        set_error("No se pudo copiar una ruta del indice");
        return -1;
    }

    entries->items[entries->count].path = path_copy;
    entries->items[entries->count].start = start;
    entries->items[entries->count].end = end;
    entries->count++;
    return (int)(entries->count - 1);
}

static int read_file(const char *path, char **out_text, size_t *out_length)
{
    FILE *file;
    long size;
    size_t read_count;
    char *text;

    file = fopen(path, "rb");
    if (file == NULL) {
        set_error("No se pudo abrir el archivo JSON");
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error("No se pudo medir el archivo JSON");
        return -1;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        set_error("No se pudo obtener el tamano del archivo JSON");
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error("No se pudo volver al inicio del archivo JSON");
        return -1;
    }

    text = (char *)malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        set_error("No se pudo reservar memoria para leer el JSON");
        return -1;
    }

    read_count = fread(text, 1, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        set_error("No se pudo leer todo el archivo JSON");
        return -1;
    }

    text[size] = '\0';
    *out_text = text;
    *out_length = (size_t)size;
    return 0;
}

static void skip_ws(Parser *parser)
{
    while (parser->pos < parser->length &&
           isspace((unsigned char)parser->text[parser->pos])) {
        parser->pos++;
    }
}

static char *append_char(char *buffer, size_t *length, size_t *capacity,
                         char value)
{
    char *new_buffer;
    size_t new_capacity;

    if (*length + 1 >= *capacity) {
        new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        new_buffer = (char *)realloc(buffer, new_capacity);
        if (new_buffer == NULL) {
            free(buffer);
            return NULL;
        }
        buffer = new_buffer;
        *capacity = new_capacity;
    }

    buffer[*length] = value;
    (*length)++;
    buffer[*length] = '\0';
    return buffer;
}

static int parse_string(Parser *parser, char **out_value)
{
    char *value = NULL;
    size_t value_len = 0;
    size_t value_cap = 0;

    if (parser->pos >= parser->length || parser->text[parser->pos] != '"') {
        set_error("Se esperaba una cadena JSON");
        return -1;
    }

    parser->pos++;
    while (parser->pos < parser->length) {
        char c = parser->text[parser->pos++];
        if (c == '"') {
            if (out_value != NULL) {
                if (value == NULL) {
                    value = jnx_strdup("");
                }
                *out_value = value;
            } else {
                free(value);
            }
            return 0;
        }

        if (c == '\\') {
            char escaped;
            if (parser->pos >= parser->length) {
                free(value);
                set_error("Escape incompleto dentro de una cadena JSON");
                return -1;
            }
            escaped = parser->text[parser->pos++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                c = escaped;
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u':
                if (parser->pos + 4 > parser->length) {
                    free(value);
                    set_error("Escape unicode incompleto");
                    return -1;
                }
                parser->pos += 4;
                c = '?';
                break;
            default:
                c = escaped;
                break;
            }
        }

        if (out_value != NULL) {
            value = append_char(value, &value_len, &value_cap, c);
            if (value == NULL) {
                set_error("No se pudo reservar memoria para una cadena");
                return -1;
            }
        }
    }

    free(value);
    set_error("Cadena JSON sin cierre");
    return -1;
}

static char *escape_path_component(const char *value)
{
    char *escaped = NULL;
    size_t len = 0;
    size_t cap = 0;

    while (*value != '\0') {
        if (*value == '~') {
            escaped = append_char(escaped, &len, &cap, '~');
            if (escaped != NULL) {
                escaped = append_char(escaped, &len, &cap, '0');
            }
        } else if (*value == '/') {
            escaped = append_char(escaped, &len, &cap, '~');
            if (escaped != NULL) {
                escaped = append_char(escaped, &len, &cap, '1');
            }
        } else {
            escaped = append_char(escaped, &len, &cap, *value);
        }
        if (escaped == NULL) {
            return NULL;
        }
        value++;
    }

    if (escaped == NULL) {
        escaped = jnx_strdup("");
    }
    return escaped;
}

static char *make_child_path(const char *parent, const char *component)
{
    size_t parent_len = strlen(parent);
    size_t component_len = strlen(component);
    size_t total;
    char *path;

    if (strcmp(parent, "$") == 0) {
        parent_len = 0;
    }

    total = parent_len + 1 + component_len;
    path = (char *)malloc(total + 1);
    if (path == NULL) {
        set_error("No se pudo reservar memoria para una ruta");
        return NULL;
    }

    if (parent_len > 0) {
        memcpy(path, parent, parent_len);
    }
    path[parent_len] = '/';
    memcpy(path + parent_len + 1, component, component_len);
    path[total] = '\0';
    return path;
}

static int parse_value(Parser *parser, const char *path);

static int parse_object(Parser *parser, const char *path)
{
    long start = (long)parser->pos;
    int entry_index = entries_add(&parser->entries, path, start, -1);

    if (entry_index < 0) {
        return -1;
    }

    parser->pos++;
    skip_ws(parser);
    if (parser->pos < parser->length && parser->text[parser->pos] == '}') {
        parser->pos++;
        parser->entries.items[entry_index].end = (long)parser->pos;
        return 0;
    }

    while (parser->pos < parser->length) {
        char *key = NULL;
        char *escaped_key = NULL;
        char *child_path = NULL;

        skip_ws(parser);
        if (parse_string(parser, &key) != 0) {
            return -1;
        }
        escaped_key = escape_path_component(key);
        free(key);
        if (escaped_key == NULL) {
            set_error("No se pudo construir la ruta de una llave");
            return -1;
        }
        child_path = make_child_path(path, escaped_key);
        free(escaped_key);
        if (child_path == NULL) {
            return -1;
        }

        skip_ws(parser);
        if (parser->pos >= parser->length || parser->text[parser->pos] != ':') {
            free(child_path);
            set_error("Se esperaba ':' despues de una llave");
            return -1;
        }
        parser->pos++;

        if (parse_value(parser, child_path) != 0) {
            free(child_path);
            return -1;
        }
        free(child_path);

        skip_ws(parser);
        if (parser->pos < parser->length && parser->text[parser->pos] == ',') {
            parser->pos++;
            continue;
        }
        if (parser->pos < parser->length && parser->text[parser->pos] == '}') {
            parser->pos++;
            parser->entries.items[entry_index].end = (long)parser->pos;
            return 0;
        }

        set_error("Se esperaba ',' o '}' dentro de un objeto");
        return -1;
    }

    set_error("Objeto JSON sin cierre");
    return -1;
}

static int parse_array(Parser *parser, const char *path)
{
    long start = (long)parser->pos;
    int entry_index = entries_add(&parser->entries, path, start, -1);
    int index = 0;

    if (entry_index < 0) {
        return -1;
    }

    parser->pos++;
    skip_ws(parser);
    if (parser->pos < parser->length && parser->text[parser->pos] == ']') {
        parser->pos++;
        parser->entries.items[entry_index].end = (long)parser->pos;
        return 0;
    }

    while (parser->pos < parser->length) {
        char component[32];
        char *child_path;

        snprintf(component, sizeof(component), "%d", index);
        child_path = make_child_path(path, component);
        if (child_path == NULL) {
            return -1;
        }

        if (parse_value(parser, child_path) != 0) {
            free(child_path);
            return -1;
        }
        free(child_path);
        index++;

        skip_ws(parser);
        if (parser->pos < parser->length && parser->text[parser->pos] == ',') {
            parser->pos++;
            continue;
        }
        if (parser->pos < parser->length && parser->text[parser->pos] == ']') {
            parser->pos++;
            parser->entries.items[entry_index].end = (long)parser->pos;
            return 0;
        }

        set_error("Se esperaba ',' o ']' dentro de un arreglo");
        return -1;
    }

    set_error("Arreglo JSON sin cierre");
    return -1;
}

static int parse_literal(Parser *parser, const char *literal)
{
    size_t len = strlen(literal);
    if (parser->pos + len > parser->length ||
        strncmp(parser->text + parser->pos, literal, len) != 0) {
        set_error("Literal JSON invalido");
        return -1;
    }
    parser->pos += len;
    return 0;
}

static int parse_number(Parser *parser)
{
    if (parser->pos < parser->length && parser->text[parser->pos] == '-') {
        parser->pos++;
    }

    if (parser->pos >= parser->length ||
        !isdigit((unsigned char)parser->text[parser->pos])) {
        set_error("Numero JSON invalido");
        return -1;
    }

    if (parser->text[parser->pos] == '0') {
        parser->pos++;
    } else {
        while (parser->pos < parser->length &&
               isdigit((unsigned char)parser->text[parser->pos])) {
            parser->pos++;
        }
    }

    if (parser->pos < parser->length && parser->text[parser->pos] == '.') {
        parser->pos++;
        if (parser->pos >= parser->length ||
            !isdigit((unsigned char)parser->text[parser->pos])) {
            set_error("Fraccion JSON invalida");
            return -1;
        }
        while (parser->pos < parser->length &&
               isdigit((unsigned char)parser->text[parser->pos])) {
            parser->pos++;
        }
    }

    if (parser->pos < parser->length &&
        (parser->text[parser->pos] == 'e' || parser->text[parser->pos] == 'E')) {
        parser->pos++;
        if (parser->pos < parser->length &&
            (parser->text[parser->pos] == '+' || parser->text[parser->pos] == '-')) {
            parser->pos++;
        }
        if (parser->pos >= parser->length ||
            !isdigit((unsigned char)parser->text[parser->pos])) {
            set_error("Exponente JSON invalido");
            return -1;
        }
        while (parser->pos < parser->length &&
               isdigit((unsigned char)parser->text[parser->pos])) {
            parser->pos++;
        }
    }

    return 0;
}

static int parse_value(Parser *parser, const char *path)
{
    long start;

    skip_ws(parser);
    if (parser->pos >= parser->length) {
        set_error("Se esperaba un valor JSON");
        return -1;
    }

    if (parser->text[parser->pos] == '{') {
        return parse_object(parser, path);
    }
    if (parser->text[parser->pos] == '[') {
        return parse_array(parser, path);
    }

    start = (long)parser->pos;
    if (parser->text[parser->pos] == '"') {
        if (parse_string(parser, NULL) != 0) {
            return -1;
        }
    } else if (parser->text[parser->pos] == 't') {
        if (parse_literal(parser, "true") != 0) {
            return -1;
        }
    } else if (parser->text[parser->pos] == 'f') {
        if (parse_literal(parser, "false") != 0) {
            return -1;
        }
    } else if (parser->text[parser->pos] == 'n') {
        if (parse_literal(parser, "null") != 0) {
            return -1;
        }
    } else {
        if (parse_number(parser) != 0) {
            return -1;
        }
    }

    if (entries_add(&parser->entries, path, start, (long)parser->pos) < 0) {
        return -1;
    }
    return 0;
}

int jnx_default_index_path(const char *json_path, char *buffer,
                           size_t buffer_size)
{
    const char *slash = strrchr(json_path, '/');
    const char *name_start = slash == NULL ? json_path : slash + 1;
    const char *dot = strrchr(name_start, '.');
    size_t prefix_len;

    if (buffer_size == 0) {
        return -1;
    }

    if (dot == NULL) {
        prefix_len = strlen(json_path);
    } else {
        prefix_len = (size_t)(dot - json_path);
    }

    if (prefix_len + 4 >= buffer_size) {
        set_error("La ruta del indice .jnx es demasiado larga");
        return -1;
    }

    memcpy(buffer, json_path, prefix_len);
    memcpy(buffer + prefix_len, ".jnx", 5);
    return 0;
}

int jnx_build_index(const char *json_path, const char *index_path)
{
    Parser parser;
    FILE *index_file;
    size_t i;
    char default_path[1024];

    memset(&parser, 0, sizeof(parser));
    g_error[0] = '\0';

    if (index_path == NULL) {
        if (jnx_default_index_path(json_path, default_path, sizeof(default_path)) != 0) {
            return -1;
        }
        index_path = default_path;
    }

    if (read_file(json_path, &parser.text, &parser.length) != 0) {
        return -1;
    }

    if (parse_value(&parser, "$") != 0) {
        free(parser.text);
        entries_free(&parser.entries);
        return -1;
    }

    skip_ws(&parser);
    if (parser.pos != parser.length) {
        free(parser.text);
        entries_free(&parser.entries);
        set_error("Hay contenido extra despues del JSON principal");
        return -1;
    }

    index_file = fopen(index_path, "w");
    if (index_file == NULL) {
        free(parser.text);
        entries_free(&parser.entries);
        set_error("No se pudo crear el archivo .jnx");
        return -1;
    }

    for (i = 0; i < parser.entries.count; i++) {
        fprintf(index_file, "%s\t%ld\t%ld\n",
                parser.entries.items[i].path,
                parser.entries.items[i].start,
                parser.entries.items[i].end);
    }

    fclose(index_file);
    free(parser.text);
    entries_free(&parser.entries);
    return 0;
}

char *jnx_extract_fragment(const char *json_path, long start, long end)
{
    FILE *file;
    long length;
    char *fragment;
    size_t read_count;

    if (start < 0 || end < start) {
        set_error("Rango de bytes invalido");
        return NULL;
    }

    length = end - start;
    fragment = (char *)malloc((size_t)length + 1);
    if (fragment == NULL) {
        set_error("No se pudo reservar memoria para el fragmento");
        return NULL;
    }

    file = fopen(json_path, "rb");
    if (file == NULL) {
        free(fragment);
        set_error("No se pudo abrir el JSON para extraer un fragmento");
        return NULL;
    }

    if (fseek(file, start, SEEK_SET) != 0) {
        fclose(file);
        free(fragment);
        set_error("No se pudo ubicar el fragmento en el JSON");
        return NULL;
    }

    read_count = fread(fragment, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(fragment);
        set_error("No se pudo leer el fragmento completo");
        return NULL;
    }

    fragment[length] = '\0';
    return fragment;
}

static char *normalize_expression(const char *expression)
{
    if (strcmp(expression, "$") == 0) {
        return jnx_strdup("\\$");
    }
    if (expression[0] == '$' && expression[1] != '\0') {
        return jnx_strdup(expression + 1);
    }
    return jnx_strdup(expression);
}

static char *anchor_expression(const char *expression)
{
    size_t len = strlen(expression);
    char *anchored = (char *)malloc(len + 5);
    if (anchored == NULL) {
        set_error("No se pudo reservar memoria para la expresion regular");
        return NULL;
    }
    snprintf(anchored, len + 5, "^(%s)$", expression);
    return anchored;
}

static int add_fragment(char ***items, size_t *count, size_t *capacity,
                        char *fragment)
{
    char **new_items;
    size_t new_capacity;

    if (*count == *capacity) {
        new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        new_items = (char **)realloc(*items, new_capacity * sizeof(char *));
        if (new_items == NULL) {
            set_error("No se pudo reservar memoria para los resultados");
            return -1;
        }
        *items = new_items;
        *capacity = new_capacity;
    }

    (*items)[*count] = fragment;
    (*count)++;
    return 0;
}

static void free_fragments(char **items, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static char *join_fragments(char **items, size_t count)
{
    size_t i;
    size_t total = 3;
    char *result;
    size_t offset;

    if (count == 0) {
        return jnx_strdup("[]");
    }

    if (count == 1) {
        result = jnx_strdup(items[0]);
        return result;
    }

    for (i = 0; i < count; i++) {
        total += strlen(items[i]) + 2;
    }

    result = (char *)malloc(total);
    if (result == NULL) {
        set_error("No se pudo reservar memoria para el JSON de salida");
        return NULL;
    }

    offset = 0;
    result[offset++] = '[';
    for (i = 0; i < count; i++) {
        size_t len = strlen(items[i]);
        if (i > 0) {
            result[offset++] = ',';
            result[offset++] = ' ';
        }
        memcpy(result + offset, items[i], len);
        offset += len;
    }
    result[offset++] = ']';
    result[offset] = '\0';
    return result;
}

char *jnx_search(const char *json_path, const char *index_path,
                 const char *expression, int *match_count)
{
    char default_path[1024];
    char line[4096];
    char *normalized = NULL;
    char *anchored = NULL;
    regex_t regex;
    int regex_ready = 0;
    FILE *index_file = NULL;
    char **fragments = NULL;
    size_t fragment_count = 0;
    size_t fragment_capacity = 0;
    char *result = NULL;

    g_error[0] = '\0';
    if (match_count != NULL) {
        *match_count = 0;
    }

    if (index_path == NULL) {
        if (jnx_default_index_path(json_path, default_path, sizeof(default_path)) != 0) {
            return NULL;
        }
        index_path = default_path;
    }

    normalized = normalize_expression(expression);
    if (normalized == NULL) {
        set_error("No se pudo normalizar la expresion de busqueda");
        goto cleanup;
    }
    anchored = anchor_expression(normalized);
    if (anchored == NULL) {
        goto cleanup;
    }

    if (regcomp(&regex, anchored, REG_EXTENDED) != 0) {
        set_error("La expresion regular no es valida");
        goto cleanup;
    }
    regex_ready = 1;

    index_file = fopen(index_path, "r");
    if (index_file == NULL) {
        set_error("No se pudo abrir el archivo .jnx");
        goto cleanup;
    }

    while (fgets(line, sizeof(line), index_file) != NULL) {
        char *tab1;
        char *tab2;
        char *end_text;
        long start;
        long end;

        tab1 = strchr(line, '\t');
        if (tab1 == NULL) {
            continue;
        }
        *tab1 = '\0';
        tab2 = strchr(tab1 + 1, '\t');
        if (tab2 == NULL) {
            continue;
        }
        *tab2 = '\0';
        end_text = tab2 + 1;
        end_text[strcspn(end_text, "\r\n")] = '\0';

        if (regexec(&regex, line, 0, NULL, 0) == 0) {
            char *fragment;
            start = atol(tab1 + 1);
            end = atol(end_text);
            fragment = jnx_extract_fragment(json_path, start, end);
            if (fragment == NULL) {
                goto cleanup;
            }
            if (add_fragment(&fragments, &fragment_count,
                             &fragment_capacity, fragment) != 0) {
                free(fragment);
                goto cleanup;
            }
        }
    }

    result = join_fragments(fragments, fragment_count);
    if (result != NULL && match_count != NULL) {
        *match_count = (int)fragment_count;
    }

cleanup:
    if (index_file != NULL) {
        fclose(index_file);
    }
    if (regex_ready) {
        regfree(&regex);
    }
    free(normalized);
    free(anchored);
    free_fragments(fragments, fragment_count);
    return result;
}
