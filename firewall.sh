#!/bin/bash
# firewall.sh — Abrir portas TechFix (2021 web + 2022 lojas)
[ "$EUID" -ne 0 ] && echo "Execute com sudo: sudo ./firewall.sh" && exit 1
for PORT in 2021 2022; do
    command -v ufw &>/dev/null && ufw allow $PORT/tcp && echo "[OK] UFW: porta $PORT"
    command -v firewall-cmd &>/dev/null && firewall-cmd --permanent --add-port=$PORT/tcp && firewall-cmd --reload && echo "[OK] firewalld: porta $PORT"
    command -v iptables &>/dev/null && iptables -C INPUT -p tcp --dport $PORT -j ACCEPT 2>/dev/null || \
        (iptables -I INPUT -p tcp --dport $PORT -j ACCEPT && echo "[OK] iptables: porta $PORT")
done
echo ""; echo "Portas 2021 e 2022 abertas."
echo "Web: http://$(hostname -I | awk '{print $1}'):2021"
