#!/bin/bash
# build.sh - Script de compilação para Linux/macOS

echo "=================================="
echo "COMPILANDO SISTEMA DE GESTÃO"
echo "=================================="

CXX="g++"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter"
LIBS="-lws2_32"
OUTPUT="gestao"

SRCS="main.cpp auth.cpp clientes.cpp produtos.cpp vendas.cpp orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp logs.cpp documentos.cpp ui.cpp server.cpp"

mkdir -p data docs

echo "Compilando..."
$CXX $CXXFLAGS -o $OUTPUT $SRCS $LIBS

if [ -f "$OUTPUT" ]; then
    SIZE=$(ls -lh $OUTPUT | awk '{print $5}')
    echo ""
    echo "[OK] Compilação bem-sucedida!"
    echo "Arquivo: $OUTPUT ($SIZE)"
    echo ""
    echo "Para executar:"
    echo "  ./$OUTPUT"
    exit 0
else
    echo "[ERRO] Compilação falhou"
    exit 1
fi
