#!/bin/bash
# start.sh - Arrancar o sistema
#
# Uso:
#   ./start.sh         → terminal direto
#   ./start.sh web     → servidor web porta 2021

if [ "$1" = "web" ]; then
    echo "Iniciando servidor web em http://localhost:2021 ..."
    if ! [ -f ./gestao ]; then make gestao; fi
    if ! [ -f ./webserver ]; then make webserver; fi
    ./webserver
else
    if ! [ -f ./gestao ]; then make gestao; fi
    ./gestao
fi
