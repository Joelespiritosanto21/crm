#include "api_client.h"
#include "common.h"
#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using sock_t = SOCKET;
#ifdef _WIN32
typedef int ssize_t;
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
using sock_t = int;
#endif

static bool init_sockets_once() {
#ifdef _WIN32
    static bool started = false;
    if (!started) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;
        started = true;
    }
#endif
    return true;
}

static void close_socket(sock_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static sock_t connect_host(const std::string& host, int port) {
    if (!init_sockets_once()) return -1;
    struct addrinfo hints;
    struct addrinfo* res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string sport = std::to_string(port);
    int err = getaddrinfo(host.c_str(), sport.c_str(), &hints, &res);
    if (err != 0 || !res) return -1;

    sock_t s = -1;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        if (connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        close_socket(s);
        s = -1;
    }
    freeaddrinfo(res);
    return s;
}

bool api_send_request(const std::string& host, int port, const JsonValue& req, JsonValue& resp, int /*timeout_ms*/) {
    sock_t s = connect_host(host, port);
    if (s < 0) return false;
    // attach token if configured
    JsonValue r = req;
    if (!g_api_token.empty()) r["token"] = JsonValue(g_api_token);
    std::string out = jsonSerialize(r, 0);
    out.push_back('\n');
    ssize_t sent = send(s, out.c_str(), (int)out.size(), 0);
    if (sent <= 0) { close_socket(s); return false; }

    std::string in;
    char buf[1024];
    while (true) {
        ssize_t r = recv(s, buf, sizeof(buf), 0);
        if (r <= 0) break;
        in.append(buf, buf + r);
        if (in.find('\n') != std::string::npos) break;
    }
    close_socket(s);
    if (in.empty()) return false;
    // Trim up to newline
    size_t pos = in.find('\n');
    std::string json = (pos==std::string::npos) ? in : in.substr(0,pos);
    try {
        JsonParser p(json);
        resp = p.parse();
        return true;
    } catch(...) {
        return false;
    }
}

bool api_get_clientes(const std::string& host, int port, JsonValue& clientes) {
    JsonObject req; req["action"] = JsonValue("get_clientes");
    JsonValue rreq(req);
    JsonValue resp;
    if (!api_send_request(host, port, rreq, resp)) return false;
    if (resp.isArray() || resp.isObject()) { clientes = resp; return true; }
    return false;
}

bool api_post_cliente(const std::string& host, int port, const JsonValue& cliente, JsonValue& resp) {
    JsonObject req; req["action"] = JsonValue("post_cliente"); req["cliente"] = cliente;
    JsonValue rreq(req);
    return api_send_request(host, port, rreq, resp);
}
