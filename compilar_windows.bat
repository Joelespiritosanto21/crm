@echo off
setlocal
echo.
echo  ============================================
echo   TechFix - Compilar cliente para Windows
echo   (standalone - sem necessidade de DLLs)
echo  ============================================
echo.

:: Procurar g++ nos paths tipicos do MSYS2/MinGW
set GPP=
if exist "C:\msys64\ucrt64\bin\g++.exe"  set "GPP=C:\msys64\ucrt64\bin\g++.exe"
if exist "C:\msys64\mingw64\bin\g++.exe" set "GPP=C:\msys64\mingw64\bin\g++.exe"
if exist "C:\mingw64\bin\g++.exe"        set "GPP=C:\mingw64\bin\g++.exe"

:: Verificar g++ no PATH do sistema
if "%GPP%"=="" (
    where g++ >nul 2>&1
    if not errorlevel 1 set GPP=g++
)

if "%GPP%"=="" (
    echo  [ERRO] g++ nao encontrado.
    echo.
    echo  Usar o terminal MSYS2 UCRT64 e correr:
    echo    ./compilar.sh
    echo.
    pause
    exit /b 1
)

echo  Compilador: %GPP%
"%GPP%" --version 2>&1 | findstr /i "g++"
echo.

if not exist data mkdir data
if not exist docs mkdir docs

echo  A compilar gestao_loja.exe (standalone)...
echo.

"%GPP%" -std=c++11 -O2 ^
    -Wno-unused-result -Wno-unused-variable ^
    -Wno-unused-function -Wno-unused-parameter ^
    -static -static-libgcc -static-libstdc++ ^
    -o gestao_loja.exe ^
    gestao_loja.cpp ^
    -lws2_32 -lwsock32 -liphlpapi

if errorlevel 1 (
    echo.
    echo  [ERRO] Compilacao falhou.
    echo  Usar o terminal MSYS2 UCRT64 e correr: ./compilar.sh
    pause
    exit /b 1
)

echo.
echo  ============================================
echo   [OK] gestao_loja.exe criado!
echo   Standalone - nao precisa de DLLs externas
echo  ============================================
echo.

if not exist servidor.conf (
    echo  A criar servidor.conf...
    (
        echo # TechFix - Configuracao
        echo host=IP_DO_SERVIDOR
        echo port=2022
        echo loja_id=
    ) > servidor.conf
    echo  IMPORTANTE: editar servidor.conf com o IP do servidor!
    notepad servidor.conf
) else (
    echo  servidor.conf:
    type servidor.conf
)

echo.
pause
