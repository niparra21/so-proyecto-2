# Proyecto 2 - JSON Index

Implementacion de una libreria en C y un programa de linea de comando para
indexar archivos JSON y recuperar fragmentos usando expresiones regulares sobre
rutas tipo JSON Pointer.chcoch

## Compilacion

```sh
make
```

## Uso

```sh
./jqindex build examples/datos.json
./jqindex search examples/datos.json "$/usuarios/[0-9]+/nombre"
```

Tambien se puede indicar explicitamente el archivo de indice:

```sh
./jqindex build examples/datos.json examples/datos.jnx
./jqindex search examples/datos.json "$/config/.*" examples/datos.jnx
```
