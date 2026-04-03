#include "fornecedores.h"
#include "common.h"
#include "json_utils.h"
#include "logs.h"
#include "sync_manager.h"

#include <iostream>

static const std::string FILE_FORNECEDORES = std::string(DATA_DIR) + "fornecedores.json";

void fornecedoresCriar() {
    std::string nome = lerString("  Nome do fornecedor: ");
    if (nome.empty()) return;
    std::string contacto = lerString("  Contacto (telefone/email): ");

    JsonObject f;
    f["id"] = JsonValue(generateId("fr_"));
    f["nome"] = JsonValue(nome);
    f["contacto"] = JsonValue(contacto);
    f["criado_em"] = JsonValue(dataAtual());

    JsonValue fs = jsonParseFile(FILE_FORNECEDORES);
    if (!fs.isArray()) fs = JsonValue(JsonArray{});
    fs.arr.push_back(JsonValue(f));
    jsonSaveFile(FILE_FORNECEDORES, fs);
    // Enfileirar para sincronização central
    sync_add_operation("create_fornecedor", JsonValue(f));
    logRegistar("fornecedor_criar", "nome="+nome);
    std::cout << "  Fornecedor criado.\n";
}

void fornecedoresMenu() {
    while (true) {
        std::cout << "\n--- Fornecedores ---\n";
        std::cout << "1) Criar fornecedor\n";
        std::cout << "2) Listar fornecedores\n";
        std::cout << "0) Voltar\n";
        std::cout << "Opcao: ";
        std::string o; std::getline(std::cin,o);
        if (o.empty()) continue;
        if (o[0]=='1') fornecedoresCriar();
        else if (o[0]=='2') {
            JsonValue fs = jsonParseFile(FILE_FORNECEDORES);
            if (!fs.isArray() || fs.arr.empty()) { std::cout<<"  Sem fornecedores.\n"; continue; }
            for (auto &f : fs.arr) {
                std::cout << "  " << f["id"].asString() << " - " << f["nome"].asString()
                          << " - " << f["contacto"].asString() << "\n";
            }
        } else if (o[0]=='0') break;
    }
}
