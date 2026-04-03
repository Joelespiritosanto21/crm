#!/bin/bash
# firewall.sh — Abrir porta 2021 na firewall do servidor
#
# Uso: sudo ./firewall.sh
#      sudo ./firewall.sh 8080    (outra porta)

PORT=${1:-2021}

echo ""
echo "  ╔════════════════════════════════════════════╗"
echo "  ║   TechFix — Configurar Firewall            ║"
echo "  ╚════════════════════════════════════════════╝"
echo ""
echo "  Porta alvo: $PORT"
echo ""

# Verificar se corre como root
if [ "$EUID" -ne 0 ]; then
    echo "  [!!] Execute com sudo: sudo ./firewall.sh $PORT"
    exit 1
fi

# ── UFW (Ubuntu/Debian) ──────────────────────────────────────
if command -v ufw &>/dev/null; then
    echo "  [i]  UFW detetado..."
    ufw allow $PORT/tcp
    echo "  [OK] UFW: porta $PORT aberta"
fi

# ── firewalld (CentOS/RHEL/Fedora) ──────────────────────────
if command -v firewall-cmd &>/dev/null; then
    echo "  [i]  firewalld detetado..."
    firewall-cmd --permanent --add-port=$PORT/tcp
    firewall-cmd --reload
    echo "  [OK] firewalld: porta $PORT aberta"
fi

# ── iptables (fallback universal) ───────────────────────────
if command -v iptables &>/dev/null; then
    echo "  [i]  iptables: a adicionar regra..."
    # Verificar se regra ja existe
    if ! iptables -C INPUT -p tcp --dport $PORT -j ACCEPT 2>/dev/null; then
        iptables -I INPUT -p tcp --dport $PORT -j ACCEPT
        echo "  [OK] iptables: porta $PORT aberta"
        # Tentar persistir
        if command -v iptables-save &>/dev/null; then
            if [ -d /etc/iptables ]; then
                iptables-save > /etc/iptables/rules.v4 && \
                    echo "  [OK] Regras guardadas em /etc/iptables/rules.v4"
            elif [ -d /etc/sysconfig ]; then
                iptables-save > /etc/sysconfig/iptables && \
                    echo "  [OK] Regras guardadas em /etc/sysconfig/iptables"
            fi
        fi
    else
        echo "  [i]  Regra iptables ja existia"
    fi
fi

echo ""
echo "  ── Verificar se porta esta acessivel ────────"

# Mostrar IP publico
PUBLIC_IP=$(curl -s --max-time 3 ifconfig.me 2>/dev/null || \
            curl -s --max-time 3 api.ipify.org 2>/dev/null || \
            echo "N/A")
LOCAL_IP=$(hostname -I 2>/dev/null | awk '{print $1}')

echo "  IP local  : $LOCAL_IP"
echo "  IP publico: $PUBLIC_IP"
echo ""
echo "  URLs de acesso:"
echo "    http://localhost:$PORT"
echo "    http://$LOCAL_IP:$PORT"
if [ "$PUBLIC_IP" != "N/A" ]; then
    echo "    http://$PUBLIC_IP:$PORT"
fi
echo ""
echo "  ── Estado da porta ──────────────────────────"
if command -v ss &>/dev/null; then
    ss -tlnp | grep ":$PORT" && echo "  [OK] Porta $PORT a escutar" || echo "  [i]  Porta $PORT nao encontrada (servidor ainda nao iniciado?)"
elif command -v netstat &>/dev/null; then
    netstat -tlnp | grep ":$PORT" && echo "  [OK] Porta $PORT a escutar" || echo "  [i]  Porta $PORT nao encontrada"
fi
echo ""
