#!/bin/bash
# install_server.sh — Instalar TechFix como serviço systemd
[ "$EUID" -ne 0 ] && echo "Execute com sudo: sudo ./install_server.sh" && exit 1
DIR="$(cd "$(dirname "$0")" && pwd)"
USER_SRV="${SUDO_USER:-$(whoami)}"
[ ! -f "$DIR/gestao_server" ] && cd "$DIR" && make gestao_server
cat > /etc/systemd/system/techfix.service << SVCEOF
[Unit]
Description=TechFix Servidor Central
After=network.target
[Service]
Type=simple
User=$USER_SRV
WorkingDirectory=$DIR
ExecStart=$DIR/gestao_server
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
[Install]
WantedBy=multi-user.target
SVCEOF
systemctl daemon-reload && systemctl enable techfix && systemctl start techfix
echo "[OK] Serviço TechFix instalado e iniciado"
echo "     sudo journalctl -u techfix -f"
