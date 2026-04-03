# Script para copiar o server.cpp reparado para o servidor remoto
# Ajuste o endereço IP, user e path conforme necessário

$source = "d:\CRM\projeto\server.cpp"
$destination = "root@45.131.64.57:/root/crm_test/crm/server.cpp"

Write-Host "Copiando $source para $destination..."

# Usando SCP via SSH (requer SSH instalado)
# Se não tiver SSH, pode usar RDP ou outro método

# Opção 1: Usando SCP (se tiver OpenSSH instalado)
try {
    scp -P 22 $source $destination
    Write-Host "✓ Arquivo copiado com sucesso!"
} catch {
    Write-Host "✗ Erro ao copiar: $_"
    Write-Host ""
    Write-Host "Alternativa: Copie manualmente o arquivo server.cpp"
    Write-Host "Do seu PC local (d:\CRM\projeto\server.cpp)"
    Write-Host "Para o servidor remoto (/root/crm_test/crm/server.cpp)"
}
