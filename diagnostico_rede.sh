#!/bin/bash
# diagnostico_rede.sh — Diagnosticar problemas de ligação externa
# Usar no SERVIDOR: ./diagnostico_rede.sh
# Usar no CLIENTE:  ./diagnostico_rede.sh <IP_SERVIDOR>

PORT_LOJAS=2022
PORT_WEB=2021
SRV_IP="${1:-}"

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║   TechFix — Diagnóstico de Rede                     ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# ── Se chamado com IP, testar do lado do cliente ─────────────
if [ -n "$SRV_IP" ]; then
    echo "Modo CLIENTE — a testar ligação a $SRV_IP"
    echo ""
    echo "── Teste de ping ────────────────────────────────────"
    if ping -c 2 -W 3 "$SRV_IP" >/dev/null 2>&1; then
        echo "  [OK] Ping a $SRV_IP funciona"
    else
        echo "  [!!] Ping falhou — servidor inacessível ou ICMP bloqueado"
    fi
    echo ""
    echo "── Teste porta $PORT_LOJAS (lojas) ──────────────────"
    if timeout 5 bash -c "echo '' > /dev/tcp/$SRV_IP/$PORT_LOJAS" 2>/dev/null; then
        echo "  [OK] Porta $PORT_LOJAS acessível!"
    else
        echo "  [!!] Porta $PORT_LOJAS BLOQUEADA"
        echo ""
        echo "  Causas possíveis:"
        echo "  1. gestao_server não está a correr no servidor"
        echo "  2. Firewall do SO: sudo ufw allow $PORT_LOJAS/tcp"
        echo "  3. Firewall do painel VPS (Hetzner/OVH/etc.) — ver abaixo"
        echo ""
        echo "  ── Firewall de painéis VPS ─────────────────────"
        echo "  Hetzner  : Cloud Console → Firewalls → Inbound TCP $PORT_LOJAS"
        echo "  OVH      : Manager → Bare Metal → IP → Firewall"
        echo "  Contabo  : Customer Portal → VPS → Firewall"
        echo "  Linode   : Cloud Manager → Firewalls → Rules"
        echo "  DigitalOcean: Networking → Firewalls → Inbound"
        echo "  Vultr    : Manage Server → Firewall → Add Rule TCP $PORT_LOJAS"
    fi
    echo ""
    echo "── Teste porta $PORT_WEB (web browser) ──────────────"
    if timeout 5 bash -c "echo '' > /dev/tcp/$SRV_IP/$PORT_WEB" 2>/dev/null; then
        echo "  [OK] Porta $PORT_WEB acessível — http://$SRV_IP:$PORT_WEB"
    else
        echo "  [!!] Porta $PORT_WEB BLOQUEADA"
    fi
    exit 0
fi

# ── Modo servidor ─────────────────────────────────────────────
echo "Modo SERVIDOR"
echo ""

# 1. IPs
echo "── Endereços IP desta máquina ───────────────────────────"
hostname -I 2>/dev/null | tr ' ' '\n' | grep -v '^$' | while read ip; do
    echo "  $ip"
done
PUBLIC_IP=$(curl -s --max-time 4 ifconfig.me 2>/dev/null || \
            curl -s --max-time 4 api.ipify.org 2>/dev/null || echo "N/D")
echo "  $PUBLIC_IP  ← IP público (usar este no servidor.conf dos clientes)"
echo ""

# 2. Processo a correr?
echo "── Estado do servidor TechFix ───────────────────────────"
if pgrep -x gestao_server > /dev/null 2>&1; then
    echo "  [OK] gestao_server a correr (PID: $(pgrep -x gestao_server))"
else
    echo "  [!!] gestao_server NÃO está a correr!"
    echo "       Iniciar: ./gestao_server &"
    echo "       Ou como serviço: sudo systemctl start techfix"
fi
echo ""

# 3. Portas a escutar?
echo "── Portas a escutar ─────────────────────────────────────"
for PORT in $PORT_LOJAS $PORT_WEB; do
    LISTEN=$(ss -tlnp 2>/dev/null | grep ":$PORT " || netstat -tlnp 2>/dev/null | grep ":$PORT ")
    if [ -n "$LISTEN" ]; then
        echo "  [OK] Porta $PORT a escutar"
    else
        echo "  [!!] Porta $PORT NÃO está a escutar"
    fi
done
echo ""

# 4. Firewall do SO
echo "── Firewall do sistema operativo ────────────────────────"
UFW_ACTIVE=false
FWD_ACTIVE=false
IPT_ACTIVE=false

if command -v ufw &>/dev/null; then
    UFW_STATUS=$(ufw status 2>/dev/null)
    if echo "$UFW_STATUS" | grep -q "Status: active"; then
        UFW_ACTIVE=true
        echo "  UFW está ACTIVO"
        for PORT in $PORT_LOJAS $PORT_WEB; do
            if echo "$UFW_STATUS" | grep -q "$PORT"; then
                echo "  [OK] UFW: porta $PORT permitida"
            else
                echo "  [!!] UFW: porta $PORT BLOQUEADA → sudo ufw allow $PORT/tcp"
            fi
        done
    else
        echo "  UFW inactivo (sem bloqueio de UFW)"
    fi
fi

if command -v firewall-cmd &>/dev/null && firewall-cmd --state 2>/dev/null | grep -q "running"; then
    FWD_ACTIVE=true
    echo "  firewalld está ACTIVO"
    for PORT in $PORT_LOJAS $PORT_WEB; do
        if firewall-cmd --list-ports 2>/dev/null | grep -q "$PORT/tcp"; then
            echo "  [OK] firewalld: porta $PORT permitida"
        else
            echo "  [!!] firewalld: porta $PORT BLOQUEADA → sudo firewall-cmd --permanent --add-port=$PORT/tcp && sudo firewall-cmd --reload"
        fi
    done
fi

if ! $UFW_ACTIVE && ! $FWD_ACTIVE; then
    echo "  Sem UFW/firewalld activos"
    if command -v iptables &>/dev/null; then
        for PORT in $PORT_LOJAS $PORT_WEB; do
            if iptables -C INPUT -p tcp --dport $PORT -j ACCEPT 2>/dev/null; then
                echo "  [OK] iptables: porta $PORT com regra ACCEPT"
            else
                echo "  [i]  iptables: porta $PORT sem regra explícita (pode estar OK se policy=ACCEPT)"
            fi
        done
    fi
fi
echo ""

# 5. Teste de conectividade local
echo "── Teste de conectividade local ─────────────────────────"
for PORT in $PORT_LOJAS $PORT_WEB; do
    if timeout 3 bash -c "echo '' > /dev/tcp/127.0.0.1/$PORT" 2>/dev/null; then
        echo "  [OK] localhost:$PORT responde"
    else
        echo "  [!!] localhost:$PORT não responde (servidor a correr?)"
    fi
done
echo ""

# 6. Resumo e solução
echo "── O que fazer se clientes externos não conseguem ligar ─"
echo ""
echo "  1. Confirmar que gestao_server está a correr no servidor"
echo "     sudo systemctl status techfix"
echo ""
echo "  2. Abrir portas na firewall do SO:"
echo "     sudo ./firewall.sh"
echo ""
echo "  3. Abrir portas no PAINEL DO VPS (MAIS COMUM!):"
echo "     → Entrar no painel web do teu VPS"
echo "     → Procurar: Firewall / Network / Security Groups"
echo "     → Adicionar regra: TCP Inbound porta $PORT_LOJAS e $PORT_WEB"
echo "     → O IP de origem pode ser 0.0.0.0/0 (qualquer)"
echo ""
echo "  4. No servidor.conf dos clientes, usar o IP PÚBLICO:"
echo "     host=$PUBLIC_IP"
echo ""
echo "  5. Testar do PC do cliente:"
echo "     ./diagnostico_rede.sh $PUBLIC_IP"
