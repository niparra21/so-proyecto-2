#!/bin/sh
set -eu

JSON_FILE="examples/datos.json"
INDEX_FILE="examples/datos.jnx"

./jqindex build "$JSON_FILE" "$INDEX_FILE" >/dev/null

names=$(./jqindex search "$JSON_FILE" '$/usuarios/[0-9]+/nombre' "$INDEX_FILE")
active=$(./jqindex search "$JSON_FILE" '$.*/activo' "$INDEX_FILE")
mode=$(./jqindex search "$JSON_FILE" '$/config/opciones/modo' "$INDEX_FILE")
missing=$(./jqindex search "$JSON_FILE" '$/no/existe' "$INDEX_FILE")

test "$names" = '["Ana", "Luis"]'
test "$active" = '[true, false]'
test "$mode" = '"auto"'
test "$missing" = '[]'

printf '%s\n' "Pruebas CLI: OK"
