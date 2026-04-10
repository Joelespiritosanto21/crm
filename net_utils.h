#pragma once
/*
 * net_utils.h - TCP cross-platform: Windows (MinGW) + Linux + macOS
 */

/* ── Headers de rede ────────────────────────────────────────── */
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  /* MinGW: ligar com -lws2_32 */
  typedef SOCKET     net_sock_t;
  #define NET_CLOSE(s)  closesocket(s)
  #define NET_ERRNO     (int)WSAGetLastError()
  #define NET_INVALID   INVALID_SOCKET
  /* socklen_t ja definido no winsock2.h do MinGW */
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int        net_sock_t;
  #define NET_CLOSE(s)  close(s)
  #define NET_ERRNO     errno
  #define NET_INVALID   (-1)
  /* socklen_t ja definido em sys/socket.h no Linux */
#endif

#include <string>
#include <cstring>
#include <cstdio>
#include "json_utils.h"

/* ── Inicializar / Terminar Winsock ─────────────────────────── */
inline bool netInit() {
#ifdef _WIN32
    WSADATA w;
    return WSAStartup(MAKEWORD(2,2), &w) == 0;
#else
    return true;
#endif
}
inline void netCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ── Definir timeout num socket ─────────────────────────────── */
inline void netSetTimeout(net_sock_t s, int ms) {
    /* ms=0 desactiva o timeout (bloqueante infinito) */
#ifdef _WIN32
    DWORD t = (ms > 0) ? (DWORD)ms : 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof(t));
#else
    struct timeval tv;
    tv.tv_sec  = (ms > 0) ? ms / 1000 : 0;
    tv.tv_usec = (ms > 0) ? (ms % 1000) * 1000 : 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif
}

/* ── Enviar linha JSON compacta (termina em \n) ─────────────── */
inline bool netSendLine(net_sock_t sock, const JsonValue& v) {
    std::string line = jsonCompact(v) + "\n";
    int total = (int)line.size();
    int sent  = 0;
    while (sent < total) {
        int n = send(sock, line.c_str() + sent, total - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

/* ── Ler linha JSON (bloqueante, com timeout) ───────────────── */
inline bool netReadLine(net_sock_t sock, std::string& out, int timeout_ms = 10000) {
    out.clear();
    netSetTimeout(sock, timeout_ms);
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') return !out.empty();
        out += c;
        if (out.size() > 1048576) return false;
    }
}

/* ── Parse de linha JSON ────────────────────────────────────── */
inline JsonValue netParseLine(const std::string& line) {
    try { JsonParser p(line); return p.parse(); } catch (...) { return JsonValue(); }
}

/* ── Construir request/response ─────────────────────────────── */
inline JsonValue mkReq(const std::string& cmd, const JsonObject& data = JsonObject()) {
    JsonObject r; r["cmd"] = JsonValue(cmd); r["data"] = JsonValue(data); return JsonValue(r);
}
inline JsonValue mkOk(const JsonValue& data = JsonValue(JsonObject())) {
    JsonObject r; r["ok"] = JsonValue(true); r["data"] = data; return JsonValue(r);
}
inline JsonValue mkErr(const std::string& msg) {
    JsonObject r; r["ok"] = JsonValue(false); r["erro"] = JsonValue(msg); return JsonValue(r);
}

/* ── Ligar ao servidor ──────────────────────────────────────── */
inline net_sock_t netConnect(const std::string& host, int port) {
    /* Resolver hostname para IP */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0 || !res)
        return NET_INVALID;

    net_sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == NET_INVALID) { freeaddrinfo(res); return NET_INVALID; }

    /* Timeout de ligacao: 8 segundos */
    netSetTimeout(s, 8000);

    int r = connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    if (r != 0) { NET_CLOSE(s); return NET_INVALID; }

    return s;
}
