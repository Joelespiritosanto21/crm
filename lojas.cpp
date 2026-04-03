/*
 * lojas.cpp - Implementação da gestão de lojas
 */

#include "lojas.h"
#include "logs.h"
#include <iostream>
#include <iomanip>

std::string lojaObterNome(const std::string& id) {
    JsonValue lojas = jsonParseFile(FILE_LOJAS);
    if (lojas.isArray()) {
        for (auto& l : lojas.arr)
            if (l["id"].asString()==id) return l["nome"].asString();
    }
    return id;
}

void lojasCriar() {
    if (!temPermissao("admin")) { erroPermissao(); return; }
    subtitulo("CRIAR LOJA");

    std::string nome = lerString("  Nome da loja: ");
    if (nome.empty()) { std::cout << "  Nome obrigatório.\n"; return; }
    std::string local = lerString("  Localização: ");

    JsonObject loja;
    loja["id"]          = JsonValue(generateId("loj_"));
    loja["nome"]        = JsonValue(nome);
    loja["localizacao"] = JsonValue(local);
    loja["estado"]      = JsonValue(std::string("ativo"));
    loja["criado_em"]   = JsonValue(dataAtual());

    JsonValue lojas = jsonParseFile(FILE_LOJAS);
    if (!lojas.isArray()) lojas = JsonValue(JsonArray{});
    lojas.arr.push_back(JsonValue(loja));
    jsonSaveFile(FILE_LOJAS, lojas);

    logRegistar("criar_loja", "nome=" + nome);
    std::cout << "  [OK] Loja '" << nome << "' criada.\n";
}

void lojasListar() {
    JsonValue lojas = jsonParseFile(FILE_LOJAS);
    subtitulo("LOJAS");
    if (!lojas.isArray() || lojas.arr.empty()) { std::cout << "  Sem lojas.\n"; return; }

    std::cout << std::left
              << std::setw(30) << "NOME"
              << std::setw(25) << "LOCALIZAÇÃO"
              << std::setw(10) << "ESTADO" << "\n";
    linha();
    for (auto& l : lojas.arr) {
        std::cout << std::setw(30) << l["nome"].asString()
                  << std::setw(25) << l["localizacao"].asString()
                  << std::setw(10) << l["estado"].asString() << "\n";
    }
}

void lojasEditar() {
    if (!temPermissao("admin")) { erroPermissao(); return; }
    subtitulo("EDITAR LOJA");
    lojasListar();

    std::string nome = lerString("\n  Nome da loja a editar: ");
    JsonValue lojas = jsonParseFile(FILE_LOJAS);
    if (!lojas.isArray()) return;

    for (auto& l : lojas.arr) {
        if (l["nome"].asString() == nome) {
            std::cout << "  1. Alterar nome\n  2. Alterar localização\n  3. Alterar estado\n  0. Cancelar\n";
            int op = lerInteiro("  Opção: ", 0, 3);
            if (op==1) { l["nome"] = JsonValue(lerString("  Novo nome: ")); }
            else if (op==2) { l["localizacao"] = JsonValue(lerString("  Nova localização: ")); }
            else if (op==3) {
                std::string est = lerString("  Estado (ativo/suspenso): ");
                if (est=="ativo"||est=="suspenso") l["estado"] = JsonValue(est);
            } else return;
            jsonSaveFile(FILE_LOJAS, lojas);
            logRegistar("editar_loja", "nome=" + nome);
            std::cout << "  [OK] Loja atualizada.\n";
            return;
        }
    }
    std::cout << "  Loja não encontrada.\n";
}

void lojasMenu() {
    while (true) {
        titulo("GESTÃO DE LOJAS");
        std::cout << "  1. Criar loja\n  2. Listar lojas\n  3. Editar loja\n  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 3);
        if (op==0) break;
        else if (op==1) lojasCriar();
        else if (op==2) lojasListar();
        else if (op==3) lojasEditar();
        pausar();
    }
}
