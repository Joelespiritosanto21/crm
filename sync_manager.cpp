#include "sync_manager.h"
#include "common.h"
#include "data_layer.h"
#include "api_client.h"

#include <iostream>
#include <vector>

static const std::string SYNC_QUEUE = "data/sync_queue.json";

static JsonValue load_queue() {
    JsonValue q = jsonParseFile(SYNC_QUEUE);
    if (!q.isArray()) return JsonValue(JsonArray{});
    return q;
}

static bool save_queue(const JsonValue& q) {
    return jsonSaveFile(SYNC_QUEUE, q);
}

bool sync_add_operation(const std::string& op, const JsonValue& data) {
    JsonValue q = load_queue();
    JsonArray arr;
    if (q.isArray()) arr = q.arr;
    JsonObject o;
    o["op"] = JsonValue(op);
    o["data"] = data;
    arr.push_back(JsonValue(o));
    return save_queue(JsonValue(arr));
}

void syncNow() {
    std::cout << "A iniciar sincronizacao com " << g_servidor_host << ":" << g_servidor_port << "...\n";
    // Primeiro: tentar enviar fila (na versao inicial apenas faz pull)
    JsonValue q = load_queue();
    if (q.isArray() && !q.arr.empty()) {
        std::cout << "Fila local com " << q.arr.size() << " operacoes. A enviar...\n";
        JsonObject req; req["action"] = JsonValue("sync_push"); req["ops"] = q;
        JsonValue rreq(req);
        JsonValue resp;
        if (api_send_request(g_servidor_host, g_servidor_port, rreq, resp)) {
            std::cout << "Servidor aplicou as operacoes. Atualizando local com pull...\n";
            // limpar fila
            save_queue(JsonValue(JsonArray{}));
            dl_pull_clientes_from_server(g_servidor_host, g_servidor_port);
            std::cout << "Sincronizacao concluida.\n";
            return;
        } else {
            std::cout << "Falha ao comunicar com servidor. Mantem fila local.\n";
        }
    }

    // Se nao havia fila, faz apenas pull
    if (dl_pull_clientes_from_server(g_servidor_host, g_servidor_port)) {
        std::cout << "Dados de clientes atualizados a partir do servidor.\n";
    } else {
        std::cout << "Falha ao obter dados do servidor. Verifique a ligacao.\n";
    }
}

void syncManagerMenu() {
    while (true) {
        std::cout << "\n--- Gestor de Sincronizacao ---\n";
        std::cout << "Servidor atual: " << g_servidor_host << ":" << g_servidor_port << "\n";
        std::cout << "1) Alterar servidor\n";
        std::cout << "2) Sincronizar agora\n";
        std::cout << "3) Ver estado da fila\n";
        std::cout << "9) Voltar\n";
        std::cout << "Opcao: ";
        std::string opt; std::getline(std::cin,opt);
        if (opt.empty()) continue;
        if (opt[0]=='1') {
            std::cout << "Host (enter para manter): "; std::string h; std::getline(std::cin,h);
            if (!h.empty()) g_servidor_host = h;
            std::cout << "Porta (enter para manter): "; std::string p; std::getline(std::cin,p);
            if (!p.empty()) g_servidor_port = std::stoi(p);
        } else if (opt[0]=='2') {
            syncNow();
            std::cout << "Prima ENTER para continuar..."; std::getline(std::cin,opt);
        } else if (opt[0]=='3') {
            JsonValue q = load_queue();
            size_t n = (q.isArray() ? q.arr.size() : 0);
            std::cout << "Operacoes na fila: " << n << "\n";
            if (n>0) {
                for (size_t i=0;i<q.arr.size();++i) {
                    std::string op = q.arr[i]["op"].asString();
                    std::cout << i+1 << ") " << op << "\n";
                }
            }
            std::cout << "Prima ENTER para continuar..."; std::getline(std::cin,opt);
        } else if (opt[0]=='9') {
            break;
        }
    }
}
