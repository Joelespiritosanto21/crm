@echo off
setlocal EnableDelayedExpansion
echo.
echo  ============================================
echo   TechFix - Instalacao do Cliente de Loja
echo  ============================================
echo.

if not exist gestao_loja.exe (
    echo  [ERRO] gestao_loja.exe nao encontrado.
    echo         Compilar primeiro com: compilar_windows.bat
    pause
    exit /b 1
)

set "INSTALL=%USERPROFILE%\TechFix"
echo  Pasta de instalacao: %INSTALL%
echo.

if not exist "%INSTALL%" mkdir "%INSTALL%"
if not exist "%INSTALL%\data" mkdir "%INSTALL%\data"
if not exist "%INSTALL%\docs" mkdir "%INSTALL%\docs"

copy /Y gestao_loja.exe "%INSTALL%\" >nul
echo  [OK] gestao_loja.exe copiado

echo.
echo  === Configuracao do Servidor ===
echo.
set /p "SRV_HOST=  IP do servidor (ex: 45.131.64.57): "
set /p "SRV_LOJA=  ID desta loja (obter no admin, pode deixar em branco): "

(
echo # TechFix - Configuracao
echo host=!SRV_HOST!
echo port=2022
echo loja_id=!SRV_LOJA!
) > "%INSTALL%\servidor.conf"

echo  [OK] servidor.conf criado

echo.
echo  A criar atalho no Ambiente de Trabalho...

set "SHORTCUT=%USERPROFILE%\Desktop\TechFix.lnk"
powershell -NoProfile -Command ^
  "$s=(New-Object -COM WScript.Shell).CreateShortcut('%SHORTCUT%');" ^
  "$s.TargetPath='%INSTALL%\gestao_loja.exe';" ^
  "$s.WorkingDirectory='%INSTALL%';" ^
  "$s.Description='TechFix Sistema de Gestao';" ^
  "$s.Save()" 2>nul

if exist "%SHORTCUT%" (
    echo  [OK] Atalho criado no Ambiente de Trabalho
) else (
    echo  [i]  Atalho nao criado automaticamente.
    echo       Criar manualmente a partir de: %INSTALL%
)

echo.
echo  ============================================
echo   Instalacao concluida!
echo.
echo   Pasta: %INSTALL%
echo   Servidor: !SRV_HOST!:2022
echo  ============================================
echo.
set /p "START=  Iniciar o sistema agora? (S/N): "
if /i "!START!"=="S" (
    cd /d "%INSTALL%"
    start "" gestao_loja.exe
)
pause
