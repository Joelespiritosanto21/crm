#pragma once
/*
 * net_utils.h - Utilitarios de rede TCP
 * Usados pelo servidor e pelo cliente de loja
 */

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib,"ws2_32.lib")
  typedef int socklen_t;
  #define NET_CLOSE(s) closesocket(s)
  #define NET_ERRNO    WSAGetLastError()
  typedef SOCKET net_sock_t;
  #define NET_INVALID  INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #define NET_CLOSE(s) close(s)
  #define NET_ERRNO    errno
  typedef int net_sock_t;
  #define NET_INVALID  (-1)
#endif

#include <string>
#include <cstring>
#include "json_utils.h"

/* ── Inicializar Winsock (no-op no Linux) ─────────────────── */
inline bool netInit() {
#ifdef _WIN32
    WSADATA w; return WSAStartup(MAKEWORD(2,2),&w)==0;
#else
    return true;
#endif
}
inline void netCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ── Enviar linha JSON (formato compacto — uma linha por mensagem) ── */
inline bool netSendLine(net_sock_t sock, const JsonValue& v) {
    std::string line = jsonCompact(v) + "\n";
    size_t sent = 0;
    while (sent < line.size()) {
        int n = send(sock, line.c_str()+sent, (int)(line.size()-sent), 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

/* ── Ler linha JSON (bloqueante) ──────────────────────────── */
inline bool netReadLine(net_sock_t sock, std::string& out, int timeout_ms = 10000) {
    out.clear();
#ifndef _WIN32
    /* timeout via setsockopt */
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
#endif
    char buf[1];
    while (true) {
        int n = recv(sock, buf, 1, 0);
        if (n <= 0) return false;
        if (buf[0] == '\n') return !out.empty();
        out += buf[0];
        if (out.size() > 1048576) return false; /* seguranca: max 1MB */
    }
}

/* ── Parse da linha recebida ──────────────────────────────── */
inline JsonValue netParseLine(const std::string& line) {
    try {
        JsonParser p(line);
        return p.parse();
    } catch (...) {
        return JsonValue();
    }
}

/* ── Construir request ────────────────────────────────────── */
inline JsonValue mkReq(const std::string& cmd, const JsonObject& data = JsonObject()) {
    JsonObject r;
    r["cmd"]  = JsonValue(cmd);
    r["data"] = JsonValue(data);
    return JsonValue(r);
}

/* ── Construir response ───────────────────────────────────── */
inline JsonValue mkOk(const JsonValue& data = JsonValue(JsonObject())) {
    JsonObject r;
    r["ok"]   = JsonValue(true);
    r["data"] = data;
    return JsonValue(r);
}
inline JsonValue mkErr(const std::string& msg) {
    JsonObject r;
    r["ok"]   = JsonValue(false);
    r["erro"] = JsonValue(msg);
    return JsonValue(r);
}

/* ── Ligar ao servidor (para o cliente de loja) ───────────── */
inline net_sock_t netConnect(const std::string& host, int port) {
    net_sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == NET_INVALID) return NET_INVALID;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    /* Resolver hostname */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), NULL, &hints, &res) == 0) {
        struct sockaddr_in* sa = (struct sockaddr_in*)res->ai_addr;
        addr.sin_addr = sa->sin_addr;
        freeaddrinfo(res);
    } else {
        NET_CLOSE(s); return NET_INVALID;
    }

    /* Timeout de ligacao: 5 segundos */
#ifndef _WIN32
    struct timeval tv; tv.tv_sec=5; tv.tv_usec=0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        NET_CLOSE(s); return NET_INVALID;
    }
    return s;
}
