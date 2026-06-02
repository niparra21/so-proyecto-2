# Proyecto 2 - JSON Index

Implementacion de una libreria en C y un programa de linea de comando para
indexar archivos JSON y recuperar fragmentos usando expresiones regulares sobre
rutas tipo JSON Pointer.

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

## Pruebas

```sh
make test
```

## Documentacion

El informe formal esta en `docs/documentacion.md` y se puede generar en PDF con:

```sh
make doc
```

## Formato `.jnx`

El indice se guarda en texto plano con una entrada por linea:

```text
ruta<TAB>inicio<TAB>fin
```

`inicio` y `fin` son posiciones de byte en el JSON original. `fin` es exclusivo,
por lo que la longitud del fragmento es `fin - inicio`.
