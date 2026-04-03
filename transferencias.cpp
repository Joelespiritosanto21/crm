#include "transferencias.h"
#include "common.h"
#include "json_utils.h"
#include "logs.h"
#include "sync_manager.h"

#include <iostream>

static const std::string FILE_TRANSFERENCIAS = std::string(DATA_DIR) + "transferencias.json";

void transferenciaCriar() {
    std::string target = lerString("  Loja destino (id): ");
    if (target.empty()) return;
    std::string ean = lerString("  EAN do produto: ");
    if (ean.empty()) return;
    int qty = lerInteiro("  Quantidade: ", 1, 999999);

    JsonObject t;
    t["id"] = JsonValue(generateId("tr_"));
    t["origem_loja"] = JsonValue(g_sessao.loja_id);
    t["destino_loja"] = JsonValue(target);
    t["ean"] = JsonValue(ean);
    t["quantidade"] = JsonValue((long long)qty);
    t["estado"] = JsonValue(std::string("pendente"));
    t["data"] = JsonValue(dataAtual());

    JsonValue tr = jsonParseFile(FILE_TRANSFERENCIAS);
    if (!tr.isArray()) tr = JsonValue(JsonArray{});
    tr.arr.push_back(JsonValue(t));
    jsonSaveFile(FILE_TRANSFERENCIAS, tr);
    logRegistar("transferencia_create", "origem="+g_sessao.loja_id+" destino="+target+" ean="+ean+" q="+std::to_string(qty));

    // Enfileira operação para sincronização central
    sync_add_operation("create_transferencia", JsonValue(t));

    std::cout << "  Pedido de transferência registado (pendente).\n";
}

void transferenciasMenu() {
    while (true) {
        std::cout << "\n--- Transferências entre lojas ---\n";
        std::cout << "1) Pedir transferência\n";
        std::cout << "2) Ver transferências\n";
        std::cout << "0) Voltar\n";
        std::cout << "Opcao: ";
        std::string o; std::getline(std::cin,o);
        if (o.empty()) continue;
        if (o[0]=='1') transferenciaCriar();
        else if (o[0]=='2') {
            JsonValue tr = jsonParseFile(FILE_TRANSFERENCIAS);
            if (!tr.isArray()||tr.arr.empty()) { std::cout<<"  Sem transferências.\n"; continue; }
            for (auto &t : tr.arr) {
                std::cout << "  " << t["id"].asString() << " - " << t["origem_loja"].asString()
                          << " -> " << t["destino_loja"].asString() << " : " << t["ean"].asString()
                          << " x" << t["quantidade"].asInt() << " [" << t["estado"].asString() << "]\n";
            }
        } else if (o[0]=='0') break;
    }
}
