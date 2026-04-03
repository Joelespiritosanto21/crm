/*
 * garantias.cpp - Implementação de gestão de garantias
 */

#include "garantias.h"
#include "clientes.h"
#include "logs.h"
#include <iostream>
#include <iomanip>

/* ================================================================
 * Criar garantia automaticamente
 * ================================================================ */
std::string garantiaCriarAuto(const std::string& cliente_id,
                               const std::string& item_nome,
                               int duracao_dias,
                               const std::string& referencia) {
    std::string data_inicio = dataApenasData();
    std::string data_fim    = adicionarDias(data_inicio, duracao_dias);

    JsonObject g;
    g["id"]          = JsonValue(generateId("gar_"));
    g["cliente_id"]  = JsonValue(cliente_id);
    g["item"]        = JsonValue(item_nome);
    g["referencia"]  = JsonValue(referencia);
    g["data_inicio"] = JsonValue(data_inicio);
    g["data_fim"]    = JsonValue(data_fim);
    g["duracao_dias"]= JsonValue((long long)duracao_dias);
    g["criado_em"]   = JsonValue(dataAtual());
    g["criado_por"]  = JsonValue(g_sessao.username);

    JsonValue garantias = jsonParseFile(FILE_GARANTIAS);
    if (!garantias.isArray()) garantias = JsonValue(JsonArray{});
    garantias.arr.push_back(JsonValue(g));
    jsonSaveFile(FILE_GARANTIAS, garantias);

    logRegistar("criar_garantia", "cliente=" + cliente_id + " item=" + item_nome);
    return data_fim;
}

/* ================================================================
 * Listar garantias
 * ================================================================ */
void garantiasListar() {
    JsonValue garantias = jsonParseFile(FILE_GARANTIAS);
    subtitulo("LISTA DE GARANTIAS");

    if (!garantias.isArray() || garantias.arr.empty()) {
        std::cout << "  Sem garantias registadas.\n"; return;
    }

    std::string hoje = dataApenasData();

    std::cout << std::left
              << std::setw(25) << "CLIENTE"
              << std::setw(28) << "ITEM"
              << std::setw(12) << "INÍCIO"
              << std::setw(12) << "FIM"
              << "ESTADO\n";
    linha();

    for (auto& g : garantias.arr) {
        std::string cliente_nome = clienteObterNome(g["cliente_id"].asString());
        std::string data_fim     = g["data_fim"].asString();
        std::string estado       = (data_fim >= hoje) ? "Válida" : "Expirada";

        std::cout << std::setw(25) << cliente_nome.substr(0,24)
                  << std::setw(28) << g["item"].asString().substr(0,27)
                  << std::setw(12) << g["data_inicio"].asString()
                  << std::setw(12) << data_fim
                  << estado << "\n";
    }
}

/* ================================================================
 * Verificar garantia de um cliente
 * ================================================================ */
void garantiasVerificar() {
    subtitulo("VERIFICAR GARANTIA");

    std::string busca = lerString("  NIF / Telefone do cliente: ");
    if (busca.empty()) return;

    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    std::string cliente_id, cliente_nome;
    if (clientes.isArray()) {
        for (auto& c : clientes.arr) {
            if (c["nif"].asString()==busca || c["telefone"].asString()==busca) {
                cliente_id   = c["id"].asString();
                cliente_nome = c["nome"].asString();
                break;
            }
        }
    }
    if (cliente_id.empty()) { std::cout << "  [!] Cliente não encontrado.\n"; return; }

    std::cout << "\n  Cliente: " << cliente_nome << "\n\n";
    std::string hoje = dataApenasData();

    JsonValue garantias = jsonParseFile(FILE_GARANTIAS);
    if (!garantias.isArray()) { std::cout << "  Sem garantias.\n"; return; }

    bool encontrou = false;
    for (auto& g : garantias.arr) {
        if (g["cliente_id"].asString() == cliente_id) {
            std::string data_fim = g["data_fim"].asString();
            std::string estado   = (data_fim >= hoje) ? "[VÁLIDA]" : "[EXPIRADA]";
            std::cout << "  " << estado << " "
                      << g["item"].asString()
                      << "  (" << g["data_inicio"].asString()
                      << " → " << data_fim << ")\n";
            if (!g["referencia"].asString().empty())
                std::cout << "    Ref: " << g["referencia"].asString() << "\n";
            encontrou = true;
        }
    }
    if (!encontrou) std::cout << "  Sem garantias para este cliente.\n";
}

/* ================================================================
 * Menu garantias
 * ================================================================ */
void garantiasMenu() {
    while (true) {
        titulo("GESTÃO DE GARANTIAS");
        std::cout << "  1. Listar todas as garantias\n";
        std::cout << "  2. Verificar garantia de cliente\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 2);
        if (op==0) break;
        else if (op==1) garantiasListar();
        else if (op==2) garantiasVerificar();
        pausar();
    }
}
