#!/usr/bin/env powershell
# build.ps1 - Script de compilação para Windows

$CXX = "g++"
$CXXFLAGS = @("-std=c++17", "-O2", "-Wall", "-Wextra")
$LIBS = @("-lws2_32")
$OUTPUT = "gestao.exe"

$SOURCES = @(
    "main.cpp",
    "auth.cpp",
    "clientes.cpp",
    "produtos.cpp",
    "vendas.cpp",
    "orcamentos.cpp",
    "reparacoes.cpp",
    "garantias.cpp",
    "lojas.cpp",
    "logs.cpp",
    "documentos.cpp",
    "ui.cpp"
)

# Limpar arquivo anterior
if (Test-Path $OUTPUT) {
    Remove-Item $OUTPUT -Force
    Write-Host "[INFO] Arquivo anterior removido"
}

# Construir comando
$cmd = @($CXX) + $CXXFLAGS + @("-o", $OUTPUT) + $SOURCES + $LIBS
$cmdStr = $cmd -join " "

Write-Host "================================================"
Write-Host "COMPILANDO: $OUTPUT"
Write-Host "================================================"
Write-Host $cmdStr
Write-Host ""

# Executar compilação
& $CXX @($CXXFLAGS + "-o" + $OUTPUT + $SOURCES + $LIBS) 2>&1 | ForEach-Object {
    Write-Host $_
}

# Verificação final
Write-Host ""
Write-Host "================================================"
if (Test-Path $OUTPUT) {
    $size = (Get-Item $OUTPUT).Length
    Write-Host "[OK] Compilação bem-sucedida!"
    Write-Host "[INFO] Arquivo: $OUTPUT ($size bytes)"
    Write-Host ""
    Write-Host "Para executar:"
    Write-Host "  .\$OUTPUT"
}
else {
    Write-Host "[ERRO] Compilação falhou - executável não criado"
    exit 1
}
Write-Host "================================================"

exit 0
