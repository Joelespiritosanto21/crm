@echo off
setlocal
echo.
echo  ============================================
echo   TechFix - Compilar cliente para Windows
echo  ============================================
echo.

:: Verificar g++
where g++ >nul 2>&1
if errorlevel 1 (
    echo  [ERRO] g++ nao encontrado.
    echo.
    echo  INSTALAR MinGW ^(escolher uma opcao^):
    echo.
    echo  OPCAO 1 - WinLibs ^(mais simples^):
    echo    1. Ir a https://winlibs.com/
    echo    2. Descarregar "Win64 UCRT" ZIP ^(GCC mais recente^)
    echo    3. Extrair para C:\mingw64
    echo    4. Adicionar C:\mingw64\bin ao PATH:
    echo       Win+Pause ^> Variaveis de ambiente ^> Path ^> Novo
    echo       Escrever: C:\mingw64\bin
    echo    5. Abrir NOVO cmd e correr este script novamente
    echo.
    echo  OPCAO 2 - MSYS2:
    echo    1. Ir a https://www.msys2.org/ e instalar
    echo    2. Abrir "MSYS2 MinGW 64-bit"
    echo    3. pacman -S mingw-w64-x86_64-gcc
    echo    4. Usar sempre o terminal MSYS2 para compilar
    echo.
    pause
    exit /b 1
)

:: Mostrar versao
for /f "tokens=*" %%i in ('g++ --version 2^>^&1 ^| findstr /i "g++"') do (
    echo  Compilador: %%i
)
echo.

:: Criar pastas
if not exist data mkdir data
if not exist docs mkdir docs

:: Compilar
echo  A compilar gestao_loja.exe...
echo.

g++ -std=c++11 -O2 -DWIN32 ^
    -Wno-unused-result -Wno-unused-variable ^
    -Wno-unused-function -Wno-unused-parameter ^
    -o gestao_loja.exe ^
    gestao_loja.cpp ^
    -lws2_32

if errorlevel 1 (
    echo.
    echo  [ERRO] Compilacao falhou!
    echo  Ver mensagem de erro acima.
    echo.
    echo  Problema comum: g++ nao e do MinGW nativo mas do Cygwin.
    echo  Usar o terminal "MSYS2 MinGW 64-bit" ou WinLibs.
    pause
    exit /b 1
)

echo.
echo  ============================================
echo   [OK] gestao_loja.exe criado!
echo  ============================================
echo.

:: Verificar servidor.conf
if exist servidor.conf (
    echo  servidor.conf encontrado:
    type servidor.conf
) else (
    echo  servidor.conf nao existe. A criar exemplo...
    echo # TechFix - Configuracao do servidor> servidor.conf
    echo host=IP_DO_SERVIDOR>> servidor.conf
    echo port=2022>> servidor.conf
    echo loja_id=>> servidor.conf
    echo.
    echo  EDITAR servidor.conf com o IP correcto do servidor!
)

echo.
echo  Para executar: gestao_loja.exe
echo  Para melhores cores: usar Windows Terminal ^(Microsoft Store^)
echo.
pause
