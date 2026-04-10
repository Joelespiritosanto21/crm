#!/bin/bash
# =============================================================
# techfix.sh — Gerir o servidor TechFix
#
# Uso:
#   ./techfix.sh start      Iniciar servidor
#   ./techfix.sh stop       Parar servidor
#   ./techfix.sh restart    Reiniciar servidor
#   ./techfix.sh status     Ver estado e estatisticas
#   ./techfix.sh logs       Ver logs em tempo real
#   ./techfix.sh backup     Fazer backup dos dados
#   ./techfix.sh install    Instalar como servico systemd (sudo)
#   ./techfix.sh uninstall  Remover servico systemd (sudo)
# =============================================================

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/gestao_server"
PIDFILE="$DIR/.techfix.pid"
LOGFILE="$DIR/logs/server.log"
BACKUPDIR="$DIR/backups"
SERVICE="techfix"

# ── Cores ────────────────────────────────────────────────────
GRN='\033[92m'; YLW='\033[93m'; RED='\033[91m'
BLD='\033[1m';  RST='\033[0m';  CYN='\033[96m'
ok()  { echo -e "  ${GRN}${BLD}[OK]${RST}  $*"; }
err() { echo -e "  ${RED}${BLD}[!!]${RST}  $*"; }
inf() { echo -e "  ${CYN}[i]${RST}   $*"; }
hdr() { echo -e "\n  ${BLD}$*${RST}"; echo "  $(printf '%.0s─' {1..50})"; }

# ── Verificar se servidor esta a correr ──────────────────────
is_running() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE" 2>/dev/null)
        [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null && return 0
    fi
    # Verificar tambem por nome do processo
    pgrep -x gestao_server >/dev/null 2>&1 && return 0
    return 1
}

get_pid() {
    if [ -f "$PIDFILE" ]; then
        cat "$PIDFILE" 2>/dev/null
    else
        pgrep -x gestao_server 2>/dev/null | head -1
    fi
}

# ── Compilar se necessario ───────────────────────────────────
compilar_se_necessario() {
    if [ ! -f "$BIN" ]; then
        inf "gestao_server nao encontrado. A compilar..."
        cd "$DIR"
        if ! make gestao_server 2>&1; then
            err "Compilacao falhou!"
            exit 1
        fi
        ok "Compilado com sucesso"
    fi
}

# ── START ────────────────────────────────────────────────────
cmd_start() {
    hdr "Iniciar TechFix"
    if is_running; then
        ok "Servidor ja esta a correr (PID: $(get_pid))"
        return 0
    fi
    compilar_se_necessario
    mkdir -p "$DIR/data" "$DIR/docs" "$DIR/logs"

    # Usar systemd se estiver instalado
    if systemctl is-enabled "$SERVICE" &>/dev/null; then
        sudo systemctl start "$SERVICE"
        sleep 1
        if systemctl is-active "$SERVICE" &>/dev/null; then
            ok "Servidor iniciado via systemd"
            inf "Logs: sudo journalctl -u $SERVICE -f"
        else
            err "Falha ao iniciar via systemd"
            sudo journalctl -u "$SERVICE" -n 20
        fi
        return
    fi

    # Iniciar directamente
    nohup "$BIN" >> "$LOGFILE" 2>&1 &
    echo $! > "$PIDFILE"
    sleep 2

    if is_running; then
        ok "Servidor iniciado (PID: $(cat $PIDFILE))"
        inf "Log: $LOGFILE"
        inf "Parar: ./techfix.sh stop"
    else
        err "Falha ao iniciar o servidor!"
        tail -20 "$LOGFILE" 2>/dev/null
        rm -f "$PIDFILE"
        exit 1
    fi
}

# ── STOP ─────────────────────────────────────────────────────
cmd_stop() {
    hdr "Parar TechFix"
    if ! is_running; then
        inf "Servidor nao esta a correr"
        return 0
    fi
    if systemctl is-active "$SERVICE" &>/dev/null; then
        sudo systemctl stop "$SERVICE"
        ok "Servidor parado (systemd)"
        return
    fi
    PID=$(get_pid)
    if [ -n "$PID" ]; then
        kill "$PID" 2>/dev/null
        sleep 2
        kill -9 "$PID" 2>/dev/null  # forcado se necessario
        rm -f "$PIDFILE"
        ok "Servidor parado (PID: $PID)"
    else
        pkill -x gestao_server 2>/dev/null
        rm -f "$PIDFILE"
        ok "Servidor parado"
    fi
}

# ── RESTART ──────────────────────────────────────────────────
cmd_restart() {
    cmd_stop
    sleep 1
    cmd_start
}

# ── STATUS ───────────────────────────────────────────────────
cmd_status() {
    hdr "Estado do TechFix"
    echo ""

    if is_running; then
        PID=$(get_pid)
        echo -e "  Estado  : ${GRN}${BLD}A CORRER${RST} (PID: $PID)"

        # Uptime do processo
        if [ -n "$PID" ] && command -v ps &>/dev/null; then
            UPTIME=$(ps -p "$PID" -o etime= 2>/dev/null | tr -d ' ')
            [ -n "$UPTIME" ] && echo "  Uptime  : $UPTIME"
        fi
    else
        echo -e "  Estado  : ${RED}${BLD}PARADO${RST}"
    fi

    # Portas
    echo ""
    for PORT in 2021 2022; do
        if ss -tlnp 2>/dev/null | grep -q ":$PORT " || \
           netstat -tlnp 2>/dev/null | grep -q ":$PORT "; then
            echo -e "  Porta $PORT : ${GRN}a escutar${RST}"
        else
            echo -e "  Porta $PORT : ${RED}fechada${RST}"
        fi
    done

    # IP público
    echo ""
    LOCAL=$(hostname -I 2>/dev/null | awk '{print $1}')
    PUBLIC=$(curl -s --max-time 3 ifconfig.me 2>/dev/null || echo "N/D")
    echo "  IP local  : $LOCAL"
    echo "  IP publico: $PUBLIC"
    echo "  Web       : http://$PUBLIC:2021"
    echo "  Lojas     : $PUBLIC:2022"

    # Estatisticas dos dados
    echo ""
    hdr "Base de dados"
    for FILE in clientes produtos vendas reparacoes garantias; do
        F="$DIR/data/$FILE.json"
        if [ -f "$F" ]; then
            COUNT=$(python3 -c "import json; d=json.load(open('$F')); print(len(d))" 2>/dev/null || \
                    grep -c '"id"' "$F" 2>/dev/null || echo "?")
            printf "  %-14s: %s registos\n" "$FILE" "$COUNT"
        fi
    done

    # Tamanho dos dados
    echo ""
    DATA_SIZE=$(du -sh "$DIR/data" 2>/dev/null | cut -f1)
    echo "  Tamanho dados : $DATA_SIZE"

    # Ultimo backup
    if [ -d "$BACKUPDIR" ]; then
        LAST_BK=$(ls -t "$BACKUPDIR"/*.tar.gz 2>/dev/null | head -1)
        if [ -n "$LAST_BK" ]; then
            echo "  Ultimo backup : $(basename $LAST_BK)"
        fi
    fi

    # Systemd
    echo ""
    if systemctl is-enabled "$SERVICE" &>/dev/null; then
        inf "Servico systemd instalado e activo no arranque"
    else
        inf "Nao instalado como servico (./techfix.sh install)"
    fi
    echo ""
}

# ── LOGS ─────────────────────────────────────────────────────
cmd_logs() {
    hdr "Logs do servidor"
    if systemctl is-active "$SERVICE" &>/dev/null; then
        inf "A mostrar logs do systemd (Ctrl+C para sair)..."
        journalctl -u "$SERVICE" -f
    elif [ -f "$LOGFILE" ]; then
        inf "A mostrar $LOGFILE (Ctrl+C para sair)..."
        tail -f "$LOGFILE"
    else
        err "Sem logs disponíveis. Servidor a correr?"
    fi
}

# ── BACKUP ───────────────────────────────────────────────────
cmd_backup() {
    hdr "Backup dos dados"
    mkdir -p "$BACKUPDIR"
    STAMP=$(date +%Y%m%d_%H%M%S)
    BKFILE="$BACKUPDIR/techfix_backup_$STAMP.tar.gz"

    tar -czf "$BKFILE" -C "$DIR" data/ 2>/dev/null
    if [ $? -eq 0 ]; then
        SIZE=$(du -sh "$BKFILE" | cut -f1)
        ok "Backup criado: $BKFILE ($SIZE)"

        # Manter apenas os ultimos 30 backups
        ls -t "$BACKUPDIR"/*.tar.gz 2>/dev/null | tail -n +31 | xargs rm -f 2>/dev/null
        COUNT=$(ls "$BACKUPDIR"/*.tar.gz 2>/dev/null | wc -l)
        inf "$COUNT backup(s) guardados"
    else
        err "Falha ao criar backup!"
    fi
}

# ── INSTALL como servico systemd ─────────────────────────────
cmd_install() {
    hdr "Instalar como servico systemd"
    if [ "$EUID" -ne 0 ]; then
        err "Requer sudo: sudo ./techfix.sh install"
        exit 1
    fi
    compilar_se_necessario
    USER_SRV="${SUDO_USER:-$(whoami)}"

    cat > /etc/systemd/system/$SERVICE.service << SVCEOF
[Unit]
Description=TechFix Servidor Central
After=network.target
StartLimitIntervalSec=0

[Service]
Type=simple
User=$USER_SRV
WorkingDirectory=$DIR
ExecStart=$BIN
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SVCEOF

    systemctl daemon-reload
    systemctl enable "$SERVICE"
    systemctl start "$SERVICE"
    sleep 2

    if systemctl is-active "$SERVICE" &>/dev/null; then
        ok "Servico instalado e iniciado!"
        ok "Inicia automaticamente com o sistema"
        echo ""
        inf "Comandos uteis:"
        inf "  sudo systemctl status $SERVICE"
        inf "  sudo journalctl -u $SERVICE -f"
        inf "  ./techfix.sh stop / start / restart"
    else
        err "Falha ao iniciar o servico!"
        journalctl -u "$SERVICE" -n 20
    fi
}

# ── UNINSTALL ────────────────────────────────────────────────
cmd_uninstall() {
    hdr "Remover servico systemd"
    if [ "$EUID" -ne 0 ]; then
        err "Requer sudo: sudo ./techfix.sh uninstall"
        exit 1
    fi
    systemctl stop "$SERVICE" 2>/dev/null
    systemctl disable "$SERVICE" 2>/dev/null
    rm -f /etc/systemd/system/$SERVICE.service
    systemctl daemon-reload
    ok "Servico removido"
}

# ── DISPATCHER ───────────────────────────────────────────────
echo ""
echo -e "  ${BLD}TechFix — Gestor do Servidor${RST}"
echo "  $(printf '%.0s═' {1..50})"

case "${1:-status}" in
    start)     cmd_start    ;;
    stop)      cmd_stop     ;;
    restart)   cmd_restart  ;;
    status|"") cmd_status   ;;
    logs)      cmd_logs     ;;
    backup)    cmd_backup   ;;
    install)   cmd_install  ;;
    uninstall) cmd_uninstall;;
    *)
        echo ""
        echo "  Uso: ./techfix.sh [comando]"
        echo ""
        echo "  Comandos:"
        echo "    start      Iniciar servidor"
        echo "    stop       Parar servidor"
        echo "    restart    Reiniciar servidor"
        echo "    status     Ver estado (default)"
        echo "    logs       Ver logs em tempo real"
        echo "    backup     Fazer backup dos dados"
        echo "    install    Instalar como servico systemd"
        echo "    uninstall  Remover servico systemd"
        echo ""
        ;;
esac
