#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include "json_utils.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using sock_t = SOCKET;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
using sock_t = int;
#endif

#include <sys/stat.h>

static void ensure_dir(const std::string& path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

static void close_socket(sock_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

int main(int argc, char** argv) {
    int port = 2022;
    if (argc>1) port = atoi(argv[1]);

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
#endif

    ensure_dir("server_data");

    sock_t serv = socket(AF_INET, SOCK_STREAM, 0);
    if (serv < 0) { std::cerr << "Erro a criar socket" << std::endl; return 1; }

    int opt = 1;
    setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Erro bind" << std::endl; close_socket(serv); return 1;
    }

    if (listen(serv, 10) < 0) { std::cerr << "Erro listen" << std::endl; close_socket(serv); return 1; }

    std::cout << "server_api: a ouvir na porta " << port << " (server_data/)\n";

    while (true) {
        struct sockaddr_in cliaddr; socklen_t clilen = sizeof(cliaddr);
        sock_t client = accept(serv, (struct sockaddr*)&cliaddr, &clilen);
        if (client < 0) { continue; }

        std::string in;
        char buf[1024];
        while (true) {
            ssize_t r = recv(client, buf, sizeof(buf), 0);
            if (r <= 0) break;
            in.append(buf, buf + r);
            if (in.find('\n') != std::string::npos) break;
        }
        if (in.empty()) { close_socket(client); continue; }
        size_t pos = in.find('\n');
        std::string json_str = (pos==std::string::npos) ? in : in.substr(0,pos);

        JsonValue req;
        try { JsonParser p(json_str); req = p.parse(); } catch(...) {
            JsonObject r; r["status"] = JsonValue("error"); r["msg"] = JsonValue("invalid json");
            std::string out = jsonSerialize(JsonValue(r),0) + "\n";
            send(client, out.c_str(), (int)out.size(), 0);
            close_socket(client);
            continue;
        }

        std::string action = req["action"].asString();
        if (action=="get_clientes") {
            JsonValue clientes = jsonParseFile(std::string("server_data/clientes.json"));
            if (!clientes.isArray()) clientes = jsonParseFile(std::string("data/clientes.json"));
            std::string out = jsonSerialize(clientes,0) + "\n";
            send(client, out.c_str(), (int)out.size(), 0);
        } else if (action=="post_cliente") {
            JsonValue cliente = req["cliente"];
            JsonValue clientes = jsonParseFile(std::string("server_data/clientes.json"));
            JsonArray arr;
            if (clientes.isArray()) arr = clientes.arr;
            arr.push_back(cliente);
            jsonSaveFile(std::string("server_data/clientes.json"), JsonValue(arr));
            JsonObject r; r["status"] = JsonValue("ok"); r["msg"] = JsonValue("cliente criado");
            std::string out = jsonSerialize(JsonValue(r),0) + "\n";
            send(client, out.c_str(), (int)out.size(), 0);
        } else if (action=="sync_push") {
            // Aplicar operacoes simples
            int applied = 0;
            if (req.has("ops") && req["ops"].isArray()) {
                JsonArray ops = req["ops"].arr;
                for (auto& opv : ops) {
                    std::string op = opv["op"].asString();
                    JsonValue data = opv["data"];
                    if (op=="create_cliente") {
                        JsonValue clientes = jsonParseFile(std::string("server_data/clientes.json"));
                        JsonArray arr;
                        if (clientes.isArray()) arr = clientes.arr;
                        arr.push_back(data);
                        jsonSaveFile(std::string("server_data/clientes.json"), JsonValue(arr));
                        applied++;
                    }
                    else if (op=="create_produto") {
                        JsonValue prods = jsonParseFile(std::string("server_data/produtos.json"));
                        JsonArray arr;
                        if (prods.isArray()) arr = prods.arr;
                        arr.push_back(data);
                        jsonSaveFile(std::string("server_data/produtos.json"), JsonValue(arr));
                        applied++;
                    }
                    else if (op=="create_venda") {
                        JsonValue vendas = jsonParseFile(std::string("server_data/vendas.json"));
                        JsonArray arr;
                        if (vendas.isArray()) arr = vendas.arr;
                        arr.push_back(data);
                        jsonSaveFile(std::string("server_data/vendas.json"), JsonValue(arr));
                        applied++;
                    }
                    // outras operacoes podem ser adicionadas aqui
                }
            }
            JsonObject r; r["status"] = JsonValue("ok"); r["applied"] = JsonValue((long long)applied);
            std::string out = jsonSerialize(JsonValue(r),0) + "\n";
            send(client, out.c_str(), (int)out.size(), 0);
        } else {
            JsonObject r; r["status"] = JsonValue("error"); r["msg"] = JsonValue("unknown action");
            std::string out = jsonSerialize(JsonValue(r),0) + "\n";
            send(client, out.c_str(), (int)out.size(), 0);
        }

        close_socket(client);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
