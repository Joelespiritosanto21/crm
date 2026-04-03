/*
 * logs.cpp - Implementação do sistema de logs
 */

#include "logs.h"
#include <iostream>

/* Registar uma entrada de log */
void logRegistar(const std::string& acao, const std::string& detalhe) {
    JsonValue logs = jsonParseFile(FILE_LOGS);
    if (!logs.isArray()) logs = JsonValue(JsonArray{});

    JsonObject entry;
    entry["data"]      = JsonValue(dataAtual());
    entry["utilizador"]= JsonValue(g_sessao.username.empty() ? "sistema" : g_sessao.username);
    entry["acao"]      = JsonValue(acao);
    entry["detalhe"]   = JsonValue(detalhe);

    logs.arr.push_back(JsonValue(entry));

    // Manter apenas os últimos 5000 logs
    if (logs.arr.size() > 5000) {
        logs.arr.erase(logs.arr.begin(), logs.arr.begin() + (logs.arr.size()-5000));
    }

    jsonSaveFile(FILE_LOGS, logs);
}

/* Listar logs (apenas admin/gerente) */
void logsListar() {
    if (!temPermissao("gerente")) { erroPermissao(); return; }

    JsonValue logs = jsonParseFile(FILE_LOGS);
    if (!logs.isArray() || logs.arr.empty()) {
        std::cout << "  Sem logs registados.\n";
        return;
    }

    titulo("REGISTO DE LOGS");
    // Mostrar últimos 50
    size_t inicio = logs.arr.size() > 50 ? logs.arr.size()-50 : 0;
    for (size_t i=inicio; i<logs.arr.size(); ++i) {
        auto& e = logs.arr[i];
        std::cout << "  [" << e["data"].asString() << "] "
                  << std::left << std::setw(10) << e["utilizador"].asString()
                  << " | " << e["acao"].asString();
        if (!e["detalhe"].asString().empty())
            std::cout << " - " << e["detalhe"].asString();
        std::cout << "\n";
    }
    std::cout << "  Total: " << logs.arr.size() << " entradas.\n";
}
