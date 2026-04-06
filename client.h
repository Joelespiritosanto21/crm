#pragma once
/*
 * client.h - Cliente TCP para gestao_loja
 * Wrapper que envia pedidos ao servidor e devolve respostas
 *
 * IMPORTANTE: sem servidor = sem acesso.
 * Todas as operacoes bloqueiam ate ter resposta ou falhar.
 */

#include "net_utils.h"
#include "protocolo.h"
#include "sha256.h"
#include "common.h"
#include <string>
#include <cstring>

/* ── Configuracao do cliente ─────────────────────────────── */
#define CLIENT_CONFIG_FILE  "servidor.conf"
#define CLIENT_TIMEOUT_MS   8000

struct ClientConfig {
    std::string host;
    int         port;
    std::string loja_id;

    ClientConfig() : host("127.0.0.1"), port(PROTO_PORT), loja_id("") {}
};

/* ── Estado da ligacao ───────────────────────────────────── */
struct ClientState {
    net_sock_t  sock{NET_INVALID};
    bool        ligado{false};
    ClientConfig cfg;
    /* Sessao autenticada */
    std::string username;
    std::string nome;
    std::string role;
    std::string loja_id;
    std::string loja_nome;
};

extern ClientState g_client;

/* ── Carregar configuracao ──────────────────────────────── */
inline ClientConfig carregarConfig() {
    ClientConfig cfg;
    std::ifstream f(CLIENT_CONFIG_FILE);
    if (!f.is_open()) return cfg;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0]=='#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        while (!val.empty() && (val.back()=='\r'||val.back()=='\n'||val.back()==' ')) val.pop_back();
        if (key=="host")    cfg.host    = val;
        if (key=="port")    cfg.port    = std::stoi(val);
        if (key=="loja_id") cfg.loja_id = val;
    }
    return cfg;
}

inline void guardarConfig(const ClientConfig& cfg) {
    std::ofstream f(CLIENT_CONFIG_FILE);
    f << "# TechFix - Configuracao do cliente de loja\n";
    f << "host=" << cfg.host << "\n";
    f << "port=" << cfg.port << "\n";
    f << "loja_id=" << cfg.loja_id << "\n";
}

/* ── Ligar ao servidor ──────────────────────────────────── */
inline bool clientLigar(const std::string& host, int port) {
    if (g_client.ligado && g_client.sock != NET_INVALID) {
        NET_CLOSE(g_client.sock);
        g_client.ligado = false;
    }
    g_client.sock = netConnect(host, port);
    if (g_client.sock == NET_INVALID) return false;

    /* Timeout de recepcao */
#ifndef _WIN32
    struct timeval tv; tv.tv_sec=CLIENT_TIMEOUT_MS/1000; tv.tv_usec=(CLIENT_TIMEOUT_MS%1000)*1000;
    setsockopt(g_client.sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    g_client.ligado = true;
    return true;
}

/* ── Enviar comando e receber resposta ──────────────────── */
inline JsonValue clientCmd(const std::string& cmd, const JsonObject& data = JsonObject()) {
    if (!g_client.ligado || g_client.sock == NET_INVALID)
        return mkErr("Sem ligacao ao servidor");

    JsonValue req = mkReq(cmd, data);
    if (!netSendLine(g_client.sock, req)) {
        g_client.ligado = false;
        return mkErr("Erro ao enviar pedido (servidor desligado?)");
    }

    std::string line;
    if (!netReadLine(g_client.sock, line, CLIENT_TIMEOUT_MS)) {
        g_client.ligado = false;
        return mkErr("Timeout ou servidor desligado");
    }

    return netParseLine(line);
}

/* ── Login ──────────────────────────────────────────────── */
inline bool clientLogin(const std::string& user, const std::string& pw) {
    JsonObject d;
    d["username"] = JsonValue(user);
    d["password"] = JsonValue(hashPassword(pw));   /* hash SHA-256 antes de enviar */
    d["loja_id"]  = JsonValue(g_client.cfg.loja_id);

    JsonValue resp = clientCmd(CMD_LOGIN, d);
    if (!resp["ok"].asBool()) {
        msgErr(resp["erro"].asString());
        return false;
    }

    g_client.username  = resp["data"]["username"].asString();
    g_client.nome      = resp["data"]["nome"].asString();
    g_client.role      = resp["data"]["role"].asString();
    g_client.loja_id   = resp["data"]["loja_id"].asString();
    g_client.loja_nome = resp["data"]["loja_nome"].asString();

    /* Actualizar sessao global (para UI) */
    g_sessao.username  = g_client.username;
    g_sessao.nome      = g_client.nome;
    g_sessao.role      = g_client.role;
    g_sessao.loja_id   = g_client.loja_id;
    g_sessao.loja_nome = g_client.loja_nome;
    g_sessao.autenticado = true;

    return true;
}

/* ── Mostrar erro de resposta ───────────────────────────── */
inline bool clientOk(const JsonValue& resp) {
    if (!resp["ok"].asBool()) {
        ecraErr(resp["erro"].asString());
        return false;
    }
    return true;
}

/* ── Verificar ligacao ──────────────────────────────────── */
inline bool clientVerificarLigacao() {
    if (!g_client.ligado) return false;
    /* Ping simples: tentar enviar um pedido de dashboard */
    JsonValue r = clientCmd(CMD_DASHBOARD);
    return r["ok"].asBool();
}
