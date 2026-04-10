#!/usr/bin/env bash
# compilar.sh — Compilar TechFix no MSYS2
# Gera um .exe completamente standalone (sem DLLs externas)

echo ""
echo " ============================================"
echo "  TechFix - Compilar cliente (MSYS2)"
echo " ============================================"
echo ""

# Encontrar g++ do MinGW/UCRT (nao o /usr/bin/g++ do MSYS generico)
GPP=""
for candidate in \
    "/ucrt64/bin/g++" \
    "/mingw64/bin/g++" \
    "/mingw32/bin/g++"; do
    if [ -x "$candidate" ]; then
        GPP="$candidate"
        break
    fi
done

# Fallback: g++ no PATH mas verificar que nao e o do MSYS generico
if [ -z "$GPP" ]; then
    WHICH_GPP=$(which g++ 2>/dev/null)
    if [ -n "$WHICH_GPP" ] && [[ "$WHICH_GPP" != "/usr/bin/g++" ]]; then
        GPP="$WHICH_GPP"
    fi
fi

if [ -z "$GPP" ]; then
    echo " [ERRO] g++ nao encontrado ou e o g++ errado."
    echo ""
    echo " Abrir o terminal correcto:"
    echo "   Menu Iniciar > MSYS2 UCRT64"
    echo ""
    echo " Instalar GCC:"
    echo "   pacman -S mingw-w64-ucrt-x86_64-gcc"
    echo ""
    echo " Verificar que esta no terminal certo:"
    echo "   which g++   deve mostrar /ucrt64/bin/g++"
    exit 1
fi

echo " Compilador: $GPP"
"$GPP" --version 2>&1 | head -1
echo ""

mkdir -p data docs

echo " A compilar gestao_loja.exe (standalone, sem DLLs)..."
echo ""

"$GPP" -std=c++11 -O2 \
    -Wno-unused-result -Wno-unused-variable \
    -Wno-unused-function -Wno-unused-parameter \
    -static \
    -static-libgcc \
    -static-libstdc++ \
    -o gestao_loja.exe \
    gestao_loja.cpp \
    -lws2_32 \
    -lwsock32 \
    -liphlpapi

if [ $? -ne 0 ]; then
    echo ""
    echo " [ERRO] Compilacao falhou."
    echo ""
    echo " Tentar sem -static (vai precisar de DLLs):"
    echo '   g++ -std=c++11 -O2 -o gestao_loja.exe gestao_loja.cpp -lws2_32'
    exit 1
fi

SIZE=$(du -sh gestao_loja.exe 2>/dev/null | cut -f1)
echo ""
echo " ============================================"
echo "  [OK] gestao_loja.exe criado! ($SIZE)"
echo "  Standalone — nao precisa de DLLs"
echo " ============================================"
echo ""

# Configurar servidor.conf
if [ ! -f servidor.conf ]; then
    echo " servidor.conf nao encontrado. Configurar agora:"
    echo ""
    read -p "   IP do servidor (ex: 45.131.64.57): " SRV_HOST
    read -p "   ID da loja (Enter para deixar em branco): " SRV_LOJA
    cat > servidor.conf << CONF
# TechFix - Configuracao do servidor
host=${SRV_HOST}
port=2022
loja_id=${SRV_LOJA}
CONF
    echo ""
    echo " servidor.conf criado:"
    cat servidor.conf
else
    echo " servidor.conf actual:"
    cat servidor.conf
fi

echo ""
echo " Para executar: ./gestao_loja.exe"
echo " Ou no explorador de ficheiros: duplo clique em gestao_loja.exe"
echo ""
