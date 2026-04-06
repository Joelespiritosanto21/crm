/*
 * clientes.cpp - Implementação completa de gestão de clientes
 */

#include "clientes.h"
#include "logs.h"
#include "auth.h"
#include "json_utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

/* ================================================================
 * Obter nome do cliente por ID
 * ================================================================ */
std::string clienteObterNome(const std::string& id) {
    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (clientes.isArray()) {
        for (auto& c : clientes.arr)
            if (c["id"].asString()==id) return c["nome"].asString();
    }
    return "Desconhecido";
}

/* ================================================================
 * Encontrar cliente por campo (nif, telefone, email)
 * ================================================================ */
JsonValue clienteEncontrar(const std::string& campo, const std::string& valor) {
    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (!clientes.isArray()) return JsonValue();
    for (auto& c : clientes.arr) {
        if (c[campo].asString() == valor) return c;
    }
    return JsonValue();
}

/* ================================================================
 * Criar cliente (interativo)
 * ================================================================ */
static JsonValue clienteCriarInterativo(const std::string& nif_predef="") {
    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (!clientes.isArray()) clientes = JsonValue(JsonArray{});

    std::string nome = lerString("  Nome: ");
    if (nome.empty()) { std::cout << "  Nome obrigatório.\n"; return JsonValue(); }

    std::string telefone;
    while (true) {
        telefone = lerString("  Telefone: ");
        if (telefone.empty()) break;
        // Verificar duplicado
        bool dup = false;
        for (auto& c : clientes.arr) if (c["telefone"].asString()==telefone) { dup=true; break; }
        if (dup) { std::cout << "  [!] Telefone já registado.\n"; continue; }
        break;
    }

    std::string email = lerString("  Email (opcional): ");
    std::string nif   = nif_predef.empty() ? lerString("  NIF (opcional): ") : nif_predef;

    // Verificar NIF duplicado
    if (!nif.empty()) {
        for (auto& c : clientes.arr) {
            if (c["nif"].asString()==nif) {
                std::cout << "  [!] NIF já registado. Cliente: " << c["nome"].asString() << "\n";
                return JsonValue();
            }
        }
    }

    JsonObject novo;
    novo["id"]           = JsonValue(generateId("cli_"));
    novo["nome"]         = JsonValue(nome);
    novo["telefone"]     = JsonValue(telefone);
    novo["email"]        = JsonValue(email);
    novo["nif"]          = JsonValue(nif);
    novo["estado"]       = JsonValue(std::string("ativo"));
    novo["data_registo"] = JsonValue(dataAtual());

    clientes.arr.push_back(JsonValue(novo));
    jsonSaveFile(FILE_CLIENTES, clientes);
    logRegistar("criar_cliente", "nome=" + nome + " nif=" + nif);
    std::cout << "  [OK] Cliente '" << nome << "' registado.\n";
    return JsonValue(novo);
}

/* ================================================================
 * Menu: Criar cliente
 * ================================================================ */
void clientesCriar() {
    subtitulo("CRIAR CLIENTE");
    clienteCriarInterativo();
}

/* ================================================================
 * Listar clientes
 * ================================================================ */
void clientesListar() {
    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    subtitulo("LISTA DE CLIENTES");

    if (!clientes.isArray() || clientes.arr.empty()) {
        std::cout << "  Sem clientes registados.\n"; return;
    }

    std::string filtro = lerString("  Filtrar por nome (Enter para todos): ");
    std::string filtroLow = filtro;
    std::transform(filtroLow.begin(), filtroLow.end(), filtroLow.begin(), ::tolower);

    std::cout << "\n" << std::left
              << std::setw(22) << "NOME"
              << std::setw(14) << "TELEFONE"
              << std::setw(12) << "NIF"
              << std::setw(10) << "ESTADO"
              << std::setw(20) << "REGISTO" << "\n";
    linha();

    int count = 0;
    for (auto& c : clientes.arr) {
        std::string nomeLow = c["nome"].asString();
        std::transform(nomeLow.begin(), nomeLow.end(), nomeLow.begin(), ::tolower);
        if (!filtroLow.empty() && nomeLow.find(filtroLow)==std::string::npos) continue;

        std::cout << std::setw(22) << c["nome"].asString().substr(0,21)
                  << std::setw(14) << c["telefone"].asString()
                  << std::setw(12) << c["nif"].asString()
                  << std::setw(10) << c["estado"].asString()
                  << std::setw(20) << c["data_registo"].asString().substr(0,10) << "\n";
        ++count;
    }
    std::cout << "\n  " << count << " cliente(s) encontrado(s).\n";
}

/* ================================================================
 * Editar cliente
 * ================================================================ */
void clientesEditar() {
    subtitulo("EDITAR CLIENTE");
    std::string busca = lerString("  NIF / Telefone / Email: ");
    if (busca.empty()) return;

    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (!clientes.isArray()) return;

    for (auto& c : clientes.arr) {
        if (c["nif"].asString()==busca || c["telefone"].asString()==busca || c["email"].asString()==busca) {
            std::cout << "  Cliente: " << c["nome"].asString() << "\n\n";
            std::cout << "  1. Nome: "      << c["nome"].asString()      << "\n";
            std::cout << "  2. Telefone: "  << c["telefone"].asString()  << "\n";
            std::cout << "  3. Email: "     << c["email"].asString()     << "\n";
            std::cout << "  4. NIF: "       << c["nif"].asString()       << "\n";
            std::cout << "  0. Cancelar\n";

            int op = lerInteiro("  Campo a editar: ", 0, 4);
            if (op==0) return;
            std::string novo_val = lerString("  Novo valor: ");
            if (op==1) c["nome"]     = JsonValue(novo_val);
            if (op==2) c["telefone"] = JsonValue(novo_val);
            if (op==3) c["email"]    = JsonValue(novo_val);
            if (op==4) c["nif"]      = JsonValue(novo_val);

            jsonSaveFile(FILE_CLIENTES, clientes);
            logRegistar("editar_cliente", "id=" + c["id"].asString());
            std::cout << "  [OK] Cliente atualizado.\n";
            return;
        }
    }
    std::cout << "  [!] Cliente não encontrado.\n";
}

/* ================================================================
 * Suspender cliente (nunca apagar)
 * ================================================================ */
void clientesSuspender() {
    if (!temPermissao("gerente")) { erroPermissao(); return; }
    subtitulo("SUSPENDER CLIENTE");
    if (!authReautenticar()) return; // Ação crítica

    std::string busca = lerString("  NIF do cliente: ");
    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (!clientes.isArray()) return;

    for (auto& c : clientes.arr) {
        if (c["nif"].asString()==busca) {
            std::cout << "  Cliente: " << c["nome"].asString() << "\n";
            if (!lerSimNao("  Confirmar suspensão?")) return;
            c["estado"] = JsonValue(std::string("suspenso"));
            jsonSaveFile(FILE_CLIENTES, clientes);
            logRegistar("suspender_cliente", "nif=" + busca);
            std::cout << "  [OK] Cliente suspenso.\n";
            return;
        }
    }
    std::cout << "  [!] Cliente não encontrado.\n";
}

/* ================================================================
 * Histórico do cliente
 * ================================================================ */
void clientesHistorico() {
    subtitulo("HISTÓRICO DO CLIENTE");
    std::string busca = lerString("  NIF / Telefone / Email: ");
    if (busca.empty()) return;

    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (!clientes.isArray()) return;

    std::string cliente_id;
    std::string cliente_nome;
    for (auto& c : clientes.arr) {
        if (c["nif"].asString()==busca || c["telefone"].asString()==busca || c["email"].asString()==busca) {
            cliente_id   = c["id"].asString();
            cliente_nome = c["nome"].asString();
            std::cout << "\n  Cliente: " << cliente_nome << "\n";
            std::cout << "  NIF: " << c["nif"].asString() << "  Tel: " << c["telefone"].asString() << "\n";
            std::cout << "  Email: " << c["email"].asString() << "\n";
            std::cout << "  Estado: " << c["estado"].asString() << "\n";
            break;
        }
    }
    if (cliente_id.empty()) { std::cout << "  [!] Cliente não encontrado.\n"; return; }

    // Vendas
    std::cout << "\n  --- VENDAS ---\n";
    JsonValue vendas = jsonParseFile(FILE_VENDAS);
    int nv = 0;
    if (vendas.isArray()) {
        for (auto& v : vendas.arr) {
            if (v["cliente_id"].asString()==cliente_id) {
                std::cout << "  " << v["numero"].asString()
                          << "  " << v["data"].asString().substr(0,10)
                          << "  " << std::fixed << std::setprecision(2) << v["total"].asDouble() << " EUR\n";
                ++nv;
            }
        }
    }
    if (!nv) std::cout << "  Sem vendas.\n";

    // Reparações
    std::cout << "\n  --- REPARAÇÕES ---\n";
    JsonValue reps = jsonParseFile(FILE_REPARACOES);
    int nr = 0;
    if (reps.isArray()) {
        for (auto& r : reps.arr) {
            if (r["cliente_id"].asString()==cliente_id) {
                std::cout << "  " << r["numero"].asString()
                          << "  " << r["equipamento"].asString()
                          << "  [" << r["estado"].asString() << "]\n";
                ++nr;
            }
        }
    }
    if (!nr) std::cout << "  Sem reparações.\n";

    // Garantias
    std::cout << "\n  --- GARANTIAS ---\n";
    JsonValue gars = jsonParseFile(FILE_GARANTIAS);
    int ng = 0;
    if (gars.isArray()) {
        for (auto& g : gars.arr) {
            if (g["cliente_id"].asString()==cliente_id) {
                std::cout << "  " << g["item"].asString()
                          << "  até " << g["data_fim"].asString() << "\n";
                ++ng;
            }
        }
    }
    if (!ng) std::cout << "  Sem garantias.\n";
}

/* ================================================================
 * Pesquisar (rápida)
 * ================================================================ */
void clientesPesquisar() {
    subtitulo("PESQUISAR CLIENTE");
    std::string busca = lerString("  NIF / Telefone / Email / Nome: ");
    if (busca.empty()) return;

    std::string buscaLow = busca;
    std::transform(buscaLow.begin(), buscaLow.end(), buscaLow.begin(), ::tolower);

    JsonValue clientes = jsonParseFile(FILE_CLIENTES);
    if (!clientes.isArray()) return;

    bool encontrou = false;
    for (auto& c : clientes.arr) {
        std::string nomeLow = c["nome"].asString();
        std::transform(nomeLow.begin(), nomeLow.end(), nomeLow.begin(), ::tolower);
        if (c["nif"].asString()==busca || c["telefone"].asString()==busca ||
            c["email"].asString()==busca || nomeLow.find(buscaLow)!=std::string::npos) {
            std::cout << "  " << c["nome"].asString()
                      << " | NIF: " << c["nif"].asString()
                      << " | Tel: " << c["telefone"].asString()
                      << " | " << c["estado"].asString() << "\n";
            encontrou = true;
        }
    }
    if (!encontrou) std::cout << "  Não encontrado.\n";
}

/* ================================================================
 * Fluxo de encontrar ou criar cliente (para vendas/orçamentos)
 * ================================================================ */
JsonValue clienteEncontrarOuCriar() {
    std::cout << "\n  Pesquisar cliente por:\n";
    std::cout << "  1. NIF\n  2. Telefone\n  3. Email\n  4. Criar novo\n";
    int op = lerInteiro("  Opção: ", 1, 4);

    if (op==4) {
        std::cout << "\n  --- Criar Novo Cliente ---\n";
        return clienteCriarInterativo();
    }

    std::string campo, prompt;
    if (op==1) { campo="nif";      prompt="  NIF: "; }
    if (op==2) { campo="telefone"; prompt="  Telefone: "; }
    if (op==3) { campo="email";    prompt="  Email: "; }

    std::string valor = lerString(prompt);
    JsonValue c = clienteEncontrar(campo, valor);

    if (!c.isNull()) {
        std::cout << "  Cliente encontrado: " << c["nome"].asString() << "\n";
        if (c["estado"].asString()=="suspenso") {
            std::cout << "  [!] Cliente suspenso.\n";
            return JsonValue();
        }
        return c;
    }

    std::cout << "  Cliente não encontrado. Criar novo?\n";
    if (!lerSimNao("  Confirmar")) return JsonValue();
    return clienteCriarInterativo(op==1 ? valor : "");
}

/* ================================================================
 * Menu clientes
 * ================================================================ */
void clientesMenu() {
    while (true) {
        titulo("GESTÃO DE CLIENTES");
        std::cout << "  1. Criar cliente\n";
        std::cout << "  2. Listar clientes\n";
        std::cout << "  3. Pesquisar cliente\n";
        std::cout << "  4. Editar cliente\n";
        std::cout << "  5. Histórico do cliente\n";
        std::cout << "  6. Suspender cliente\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 6);
        if (op==0) break;
        else if (op==1) clientesCriar();
        else if (op==2) clientesListar();
        else if (op==3) clientesPesquisar();
        else if (op==4) clientesEditar();
        else if (op==5) clientesHistorico();
        else if (op==6) clientesSuspender();
        pausar();
    }
}
