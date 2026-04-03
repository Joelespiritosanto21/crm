#!/bin/bash
# diagnostico.sh — Verificar porque o servidor nao e acessivel externamente
#
# Uso: ./diagnostico.sh [porta]

PORT=${1:-2021}

echo ""
echo "  ╔════════════════════════════════════════════╗"
echo "  ║   TechFix — Diagnóstico de Rede            ║"
echo "  ╚════════════════════════════════════════════╝"
echo ""

# 1. Servidor em execucao?
echo "  ── 1. Processo webserver ────────────────────"
if pgrep -x webserver > /dev/null; then
    echo "  [OK] webserver a correr (PID: $(pgrep -x webserver))"
else
    echo "  [!!] webserver NAO esta a correr!"
    echo "       Inicie com: ./webserver &"
fi
echo ""

# 2. Porta a escutar em 0.0.0.0?
echo "  ── 2. Bind da porta $PORT ──────────────────────"
if command -v ss &>/dev/null; then
    BIND=$(ss -tlnp | grep ":$PORT")
    if echo "$BIND" | grep -q "0.0.0.0:$PORT\|:::$PORT"; then
        echo "  [OK] A escutar em 0.0.0.0:$PORT (todas as interfaces)"
    elif echo "$BIND" | grep -q "127.0.0.1:$PORT"; then
        echo "  [!!] A escutar apenas em 127.0.0.1 (localhost)!"
        echo "       Isto e o problema — so acessivel localmente."
    elif [ -z "$BIND" ]; then
        echo "  [!!] Nenhum processo na porta $PORT"
    else
        echo "$BIND"
    fi
elif command -v netstat &>/dev/null; then
    netstat -tlnp 2>/dev/null | grep ":$PORT"
fi
echo ""

# 3. Firewall
echo "  ── 3. Firewall ──────────────────────────────"
if command -v ufw &>/dev/null; then
    STATUS=$(ufw status 2>/dev/null)
    if echo "$STATUS" | grep -q "Status: active"; then
        echo "  [i]  UFW ativo"
        if echo "$STATUS" | grep -q "$PORT"; then
            echo "  [OK] Porta $PORT permitida no UFW"
        else
            echo "  [!!] Porta $PORT NAO esta no UFW!"
            echo "       Corrigir: sudo ufw allow $PORT/tcp"
        fi
    else
        echo "  [OK] UFW inativo (sem bloqueio)"
    fi
fi

if command -v firewall-cmd &>/dev/null; then
    if firewall-cmd --state 2>/dev/null | grep -q running; then
        echo "  [i]  firewalld ativo"
        if firewall-cmd --list-ports 2>/dev/null | grep -q "$PORT/tcp"; then
            echo "  [OK] Porta $PORT permitida"
        else
            echo "  [!!] Porta $PORT NAO permitida!"
            echo "       Corrigir: sudo ./firewall.sh $PORT"
        fi
    fi
fi

if command -v iptables &>/dev/null; then
    if iptables -C INPUT -p tcp --dport $PORT -j ACCEPT 2>/dev/null; then
        echo "  [OK] iptables: porta $PORT permitida"
    else
        echo "  [!!] iptables: porta $PORT sem regra ACCEPT"
        echo "       Corrigir: sudo ./firewall.sh $PORT"
    fi
fi
echo ""

# 4. IPs disponiveis
echo "  ── 4. Enderecos IP ──────────────────────────"
hostname -I 2>/dev/null | tr ' ' '\n' | grep -v '^$' | while read ip; do
    echo "  Interface : $ip"
done
PUBLIC=$(curl -s --max-time 3 ifconfig.me 2>/dev/null || curl -s --max-time 3 api.ipify.org 2>/dev/null)
[ -n "$PUBLIC" ] && echo "  Publico   : $PUBLIC"
echo ""

# 5. Teste de conectividade local
echo "  ── 5. Teste de conectividade local ─────────"
if command -v curl &>/dev/null; then
    CODE=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 http://127.0.0.1:$PORT 2>/dev/null)
    if [ "$CODE" = "200" ]; then
        echo "  [OK] http://127.0.0.1:$PORT responde com 200"
    else
        echo "  [!!] http://127.0.0.1:$PORT nao responde (codigo: $CODE)"
    fi
fi
echo ""

echo "  ── Resumo ───────────────────────────────────"
echo "  Se tudo OK acima mas ainda nao acede externamente:"
echo "  1. O teu VPS pode ter firewall no painel de controlo"
echo "     (Hetzner, DigitalOcean, Vultr, OVH, etc.)"
echo "     → Adicionar regra TCP entrada porta $PORT no painel"
echo "  2. Verificar se o IP publico e o correto"
echo "  3. O operador pode bloquear a porta"
echo ""
