#include "pedidos.h"
#include "common.h"
#include "json_utils.h"
#include "logs.h"
#include "sync_manager.h"

#include <iostream>

static const std::string FILE_ENCOMENDAS = std::string(DATA_DIR) + "encomendas.json";

bool pedidosCriarParaProduto(const std::string& produto_id, int quantidade) {
    JsonObject e;
    e["id"] = JsonValue(generateId("en_"));
    e["produto_id"] = JsonValue(produto_id);
    e["quantidade"] = JsonValue((long long)quantidade);
    e["fornecedor_id"] = JsonValue();
    e["estado"] = JsonValue(std::string("pedido"));
    e["loja_id"] = JsonValue(g_sessao.loja_id);
    e["data"] = JsonValue(dataAtual());

    JsonValue en = jsonParseFile(FILE_ENCOMENDAS);
    if (!en.isArray()) en = JsonValue(JsonArray{});
    en.arr.push_back(JsonValue(e));
    jsonSaveFile(FILE_ENCOMENDAS, en);
    logRegistar("encomenda_criar", "produto="+produto_id+" q="+std::to_string(quantidade));
    // sincronizar
    sync_add_operation("create_encomenda", JsonValue(e));
    return true;
}

void pedidosMenu() {
    while (true) {
        std::cout << "\n--- Encomendas a fornecedores ---\n";
        std::cout << "1) Criar encomenda manual\n";
        std::cout << "2) Listar encomendas\n";
        std::cout << "0) Voltar\n";
        std::cout << "Opcao: ";
        std::string o; std::getline(std::cin,o);
        if (o.empty()) continue;
        if (o[0]=='1') {
            std::string prod = lerString("  Produto (id): ");
            int q = lerInteiro("  Quantidade: ", 1, 999999);
            pedidosCriarParaProduto(prod, q);
            std::cout << "  Encomenda criada.\n";
        } else if (o[0]=='2') {
            JsonValue en = jsonParseFile(FILE_ENCOMENDAS);
            if (!en.isArray() || en.arr.empty()) { std::cout << "  Sem encomendas.\n"; continue; }
            for (auto &e : en.arr) {
                std::cout << "  "<<e["id"].asString()<<" - prod="<<e["produto_id"].asString()
                          <<" q="<<e["quantidade"].asInt()<<" status="<<e["estado"].asString()<<"\n";
            }
        } else if (o[0]=='0') break;
    }
}
