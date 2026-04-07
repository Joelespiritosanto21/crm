@echo off
setlocal
echo.
echo  ============================================
echo   TechFix - Compilar cliente para Windows
echo  ============================================
echo.

where g++ >nul 2>&1
if errorlevel 1 (
    echo  [ERRO] g++ nao encontrado no PATH.
    echo.
    echo  Instalar MinGW:
    echo  1. Ir a https://winlibs.com/
    echo  2. Descarregar GCC Win64 ^(ZIP^)
    echo  3. Extrair para C:\mingw64
    echo  4. Adicionar C:\mingw64\bin ao PATH do Windows
    echo  5. Abrir novo CMD e tentar novamente
    echo.
    pause
    exit /b 1
)

echo  Compilador encontrado:
g++ --version 2>&1 | findstr /i "g++"
echo.
echo  A compilar gestao_loja.exe ...
echo.

g++ -std=c++11 -O2 -DWIN32 ^
    -Wno-unused-result -Wno-unused-variable ^
    -Wno-unused-function -Wno-unused-parameter ^
    -o gestao_loja.exe gestao_loja.cpp -lws2_32

if errorlevel 1 (
    echo.
    echo  [ERRO] Compilacao falhou. Ver mensagem acima.
    pause
    exit /b 1
)

echo.
echo  ============================================
echo   [OK] gestao_loja.exe criado com sucesso!
echo  ============================================
echo.
echo  Proximo passo:
if not exist servidor.conf (
    echo  Criar ficheiro servidor.conf com:
    echo    host=IP_DO_SERVIDOR
    echo    port=2022
    echo    loja_id=loj_XXXXXXXX
) else (
    echo  servidor.conf ja existe. Pronto a usar.
)
echo.
echo  Executar: gestao_loja.exe
echo.
pause
