#!/bin/bash
# firewall.sh — Abrir portas TechFix em todos os sistemas de firewall
# Uso: sudo ./firewall.sh

[ "$EUID" -ne 0 ] && echo "Execute com sudo: sudo ./firewall.sh" && exit 1

PORTS="2021 2022"
echo ""
echo "  TechFix — Configurar firewall"
echo "  Portas: $PORTS"
echo ""

for PORT in $PORTS; do
    # UFW
    if command -v ufw &>/dev/null && ufw status | grep -q "Status: active"; then
        ufw allow "$PORT/tcp" >/dev/null 2>&1 && echo "  [OK] UFW: porta $PORT"
    fi
    # firewalld
    if command -v firewall-cmd &>/dev/null && firewall-cmd --state 2>/dev/null | grep -q running; then
        firewall-cmd --permanent --add-port="$PORT/tcp" >/dev/null 2>&1
        echo "  [OK] firewalld: porta $PORT"
    fi
    # iptables
    if command -v iptables &>/dev/null; then
        iptables -C INPUT -p tcp --dport "$PORT" -j ACCEPT 2>/dev/null || \
            iptables -I INPUT -p tcp --dport "$PORT" -j ACCEPT && echo "  [OK] iptables: porta $PORT"
    fi
done

# Recarregar firewalld se necessário
command -v firewall-cmd &>/dev/null && firewall-cmd --reload >/dev/null 2>&1

echo ""
PUBLIC=$(curl -s --max-time 3 ifconfig.me 2>/dev/null || echo "N/D")
LOCAL=$(hostname -I 2>/dev/null | awk '{print $1}')
echo "  IP público  : $PUBLIC"
echo "  IP local    : $LOCAL"
echo ""
echo "  No servidor.conf das lojas usar: host=$PUBLIC"
echo ""
echo "  IMPORTANTE: Se o VPS tem firewall no painel web"
echo "  (Hetzner/OVH/Contabo/DigitalOcean/Vultr), abrir"
echo "  as portas 2021 e 2022 TCP também nesse painel!"
