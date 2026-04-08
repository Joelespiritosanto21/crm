#pragma once
/*
 * client.h - Cliente TCP para gestao_loja (cross-platform)
 */

#include "net_utils.h"
#include "protocolo.h"
#include "sha256.h"
#include "common.h"
#include <string>
#include <fstream>
#include <sstream>

#define CLIENT_CONFIG_FILE  "servidor.conf"
#define CLIENT_TIMEOUT_MS   10000

struct ClientConfig {
    std::string host;
    int         port;
    std::string loja_id;
    ClientConfig() : host(""), port(PROTO_PORT), loja_id("") {}
};

struct ClientState {
    net_sock_t  sock;
    bool        ligado;
    ClientConfig cfg;
    std::string username, nome, role, loja_id, loja_nome;
    ClientState() : sock(NET_INVALID), ligado(false) {}
};
extern ClientState g_client;

/* ── Carregar / guardar configuracao ──────────────────────── */
inline ClientConfig carregarConfig() {
    ClientConfig cfg;
    std::ifstream f(CLIENT_CONFIG_FILE);
    if (!f.is_open()) return cfg;
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back()=='\r'||line.back()=='\n'||line.back()==' '))
            line.pop_back();
        if (line.empty() || line[0]=='#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        if (key=="host")    cfg.host    = val;
        if (key=="port")    { try { cfg.port = std::stoi(val); } catch (...) {} }
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

/* ── Ligar ao servidor ────────────────────────────────────── */
inline bool clientLigar(const std::string& host, int port) {
    if (g_client.ligado && g_client.sock != NET_INVALID) {
        NET_CLOSE(g_client.sock);
        g_client.ligado = false;
        g_client.sock = NET_INVALID;
    }
    g_client.sock = netConnect(host, port);
    if (g_client.sock == NET_INVALID) return false;
    netSetTimeout(g_client.sock, CLIENT_TIMEOUT_MS);
    g_client.ligado = true;
    return true;
}

/* ── Enviar comando ───────────────────────────────────────── */
inline JsonValue clientCmd(const std::string& cmd, const JsonObject& data = JsonObject()) {
    if (!g_client.ligado || g_client.sock == NET_INVALID)
        return mkErr("Sem ligacao ao servidor. Reinicie o programa.");
    JsonValue req = mkReq(cmd, data);
    if (!netSendLine(g_client.sock, req)) {
        g_client.ligado = false;
        return mkErr("Ligacao perdida. Reinicie o programa.");
    }
    std::string line;
    if (!netReadLine(g_client.sock, line, CLIENT_TIMEOUT_MS)) {
        g_client.ligado = false;
        return mkErr("Timeout ou servidor sem resposta.");
    }
    return netParseLine(line);
}

/* ── Login ────────────────────────────────────────────────── */
inline bool clientLogin(const std::string& user, const std::string& pw) {
    JsonObject d;
    d["username"] = JsonValue(user);
    d["password"] = JsonValue(hashPassword(pw));
    d["loja_id"]  = JsonValue(g_client.cfg.loja_id);
    JsonValue resp = clientCmd(CMD_LOGIN, d);
    if (!resp["ok"].asBool()) { ecraErr(resp["erro"].asString()); return false; }
    g_client.username  = resp["data"]["username"].asString();
    g_client.nome      = resp["data"]["nome"].asString();
    g_client.role      = resp["data"]["role"].asString();
    g_client.loja_id   = resp["data"]["loja_id"].asString();
    g_client.loja_nome = resp["data"]["loja_nome"].asString();
    g_sessao.username   = g_client.username;
    g_sessao.nome       = g_client.nome;
    g_sessao.role       = g_client.role;
    g_sessao.loja_id    = g_client.loja_id;
    g_sessao.loja_nome  = g_client.loja_nome;
    g_sessao.autenticado= true;
    return true;
}

inline bool clientOk(const JsonValue& r) {
    if (!r["ok"].asBool()) { ecraErr(r["erro"].asString().empty()?"Erro desconhecido":r["erro"].asString()); return false; }
    return true;
}
