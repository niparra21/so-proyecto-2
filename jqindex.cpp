#include "libjsonindex.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static void print_usage(const char *program)
{
    std::cerr
        << "Uso:\n"
        << "  " << program << " build <archivo.json> [indice.jnx]\n"
        << "  " << program << " search <archivo.json> <expresion> [indice.jnx]\n"
        << "\nEjemplos:\n"
        << "  " << program << " build datos.json\n"
        << "  " << program << " search datos.json \"$/usuarios/[0-9]+/nombre\"\n";
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];
    const char *json_path = argv[2];
    const char *index_path = nullptr;

    if (std::strcmp(command, "build") == 0) {
        if (argc > 4) {
            print_usage(argv[0]);
            return 1;
        }
        if (argc == 4) {
            index_path = argv[3];
        }
        if (jnx_build_index(json_path, index_path) != 0) {
            std::cerr << "Error: " << jnx_last_error() << "\n";
            return 2;
        }

        char default_index[1024];
        if (index_path == nullptr &&
            jnx_default_index_path(json_path, default_index,
                                   sizeof(default_index)) == 0) {
            std::cout << "Indice generado: " << default_index << "\n";
        } else {
            std::cout << "Indice generado: " << index_path << "\n";
        }
        return 0;
    }

    if (std::strcmp(command, "search") == 0) {
        if (argc < 4 || argc > 5) {
            print_usage(argv[0]);
            return 1;
        }
        const char *expression = argv[3];
        if (argc == 5) {
            index_path = argv[4];
        }

        int matches = 0;
        char *result = jnx_search(json_path, index_path, expression, &matches);
        if (result == nullptr) {
            std::cerr << "Error: " << jnx_last_error() << "\n";
            return 2;
        }

        std::cout << result << "\n";
        jnx_free(result);
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
