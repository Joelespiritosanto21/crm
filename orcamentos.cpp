/*
 * orcamentos.cpp - Implementação de orçamentos
 */

#include "orcamentos.h"
#include "clientes.h"
#include "produtos.h"
#include "vendas.h"
#include "garantias.h"
#include "documentos.h"
#include "logs.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

static int orcamentosProximoNumero() {
    JsonValue orcs = jsonParseFile(FILE_ORCAMENTOS);
    if (!orcs.isArray() || orcs.arr.empty()) return 1;
    int max = 0;
    for (auto& o : orcs.arr) {
        std::string num = o["numero"].asString();
        if (num.size() > 4) {
            try { int n = std::stoi(num.substr(4)); if (n>max) max=n; } catch(...) {}
        }
    }
    return max + 1;
}

/* ================================================================
 * Criar orçamento
 * ================================================================ */
void orcamentosCriar() {
    titulo("NOVO ORÇAMENTO");

    JsonValue cliente = clienteEncontrarOuCriar();
    if (cliente.isNull()) { std::cout << "  Cancelado.\n"; return; }

    std::string cliente_id   = cliente["id"].asString();
    std::string cliente_nome = cliente["nome"].asString();
    std::cout << "\n  Cliente: " << cliente_nome << "\n";

    JsonArray itens;
    double total = 0.0;

    while (true) {
        subtitulo("ADICIONAR ITEM AO ORÇAMENTO");
        std::cout << "  1. Pesquisar produto por EAN\n";
        std::cout << "  2. Pesquisar produto por nome\n";
        std::cout << "  3. Adicionar item manual (serviço)\n";
        std::cout << "  0. Finalizar\n";
        int op = lerInteiro("  Opção: ", 0, 3);
        if (op==0) break;

        std::string item_nome;
        double      item_preco = 0.0;
        int         item_qty   = 1;
        std::string produto_id;
        bool        tem_garantia    = false;
        int         duracao_garantia = 0;

        if (op==1) {
            std::string ean = lerString("  EAN: ");
            JsonValue p = produtoEncontrarPorEAN(ean);
            if (p.isNull()) { std::cout << "  [!] Não encontrado.\n"; continue; }
            item_nome         = p["nome"].asString();
            item_preco        = p["preco"].asDouble();
            produto_id        = p["id"].asString();
            tem_garantia      = p["tem_garantia"].asBool();
            duracao_garantia  = (int)p["duracao_garantia"].asInt();
        } else if (op==2) {
            std::string nome = lerString("  Nome: ");
            std::string nomeLow = nome;
            std::transform(nomeLow.begin(), nomeLow.end(), nomeLow.begin(), ::tolower);
            JsonValue prods = jsonParseFile(FILE_PRODUTOS);
            std::vector<JsonValue*> enc;
            if (prods.isArray()) for (auto& p : prods.arr) {
                if (!p["ativo"].asBool()) continue;
                std::string pn = p["nome"].asString();
                std::transform(pn.begin(), pn.end(), pn.begin(), ::tolower);
                if (pn.find(nomeLow)!=std::string::npos) enc.push_back(&p);
            }
            if (enc.empty()) { std::cout << "  [!] Nenhum produto.\n"; continue; }
            for (size_t i=0; i<enc.size()&&i<10; ++i)
                std::cout << "  " << (i+1) << ". " << (*enc[i])["nome"].asString()
                          << "  " << std::fixed << std::setprecision(2)
                          << (*enc[i])["preco"].asDouble() << " EUR\n";
            int sel = lerInteiro("  Selecionar: ", 1, (int)std::min(enc.size(),(size_t)10));
            item_nome        = (*enc[sel-1])["nome"].asString();
            item_preco       = (*enc[sel-1])["preco"].asDouble();
            produto_id       = (*enc[sel-1])["id"].asString();
            tem_garantia     = (*enc[sel-1])["tem_garantia"].asBool();
            duracao_garantia = (int)(*enc[sel-1])["duracao_garantia"].asInt();
        } else {
            item_nome  = lerString("  Descrição do serviço: ");
            item_preco = lerDouble("  Preço estimado: ");
        }

        item_qty = lerInteiro("  Quantidade: ", 1, 9999);

        JsonObject item;
        item["produto_id"]      = JsonValue(produto_id);
        item["nome"]            = JsonValue(item_nome);
        item["quantidade"]      = JsonValue((long long)item_qty);
        item["preco_unitario"]  = JsonValue(item_preco);
        item["subtotal"]        = JsonValue(item_preco * item_qty);
        item["tem_garantia"]    = JsonValue(tem_garantia);
        item["duracao_garantia"]= JsonValue((long long)duracao_garantia);

        itens.push_back(JsonValue(item));
        total += item_preco * item_qty;
        std::cout << "  [+] " << item_nome << " x" << item_qty
                  << " = " << std::fixed << std::setprecision(2) << (item_preco*item_qty) << " EUR\n";
    }

    if (itens.empty()) { std::cout << "  Sem itens. Cancelado.\n"; return; }

    std::string observacoes = lerString("  Observações (opcional): ");

    int seq = orcamentosProximoNumero();
    std::string numero = gerarNumeroOrcamento(seq);

    JsonObject orc;
    orc["id"]          = JsonValue(generateId("orc_"));
    orc["numero"]      = JsonValue(numero);
    orc["cliente_id"]  = JsonValue(cliente_id);
    orc["itens"]       = JsonValue(itens);
    orc["total"]       = JsonValue(total);
    orc["estado"]      = JsonValue(std::string("pendente"));
    orc["observacoes"] = JsonValue(observacoes);
    orc["data"]        = JsonValue(dataAtual());
    orc["criado_por"]  = JsonValue(g_sessao.username);
    orc["fatura_id"]   = JsonValue(std::string(""));

    JsonValue orcs = jsonParseFile(FILE_ORCAMENTOS);
    if (!orcs.isArray()) orcs = JsonValue(JsonArray{});
    orcs.arr.push_back(JsonValue(orc));
    jsonSaveFile(FILE_ORCAMENTOS, orcs);

    logRegistar("criar_orcamento", "numero=" + numero + " cliente=" + cliente_nome);
    std::cout << "\n  [OK] Orçamento " << numero << " criado.\n";
    std::cout << "  Total: " << std::fixed << std::setprecision(2) << total << " EUR\n";

    std::string fp = docGerarOrcamento(JsonValue(orc));
    docApresentarOpcoes(fp, "Orçamento");
}

/* ================================================================
 * Listar orçamentos
 * ================================================================ */
void orcamentosListar() {
    JsonValue orcs = jsonParseFile(FILE_ORCAMENTOS);
    subtitulo("LISTA DE ORÇAMENTOS");
    if (!orcs.isArray() || orcs.arr.empty()) { std::cout << "  Sem orçamentos.\n"; return; }

    std::cout << std::left
              << std::setw(14) << "NÚMERO"
              << std::setw(22) << "CLIENTE"
              << std::setw(12) << "ESTADO"
              << std::setw(12) << "TOTAL"
              << "DATA\n";
    linha();

    for (auto& o : orcs.arr) {
        std::string cn = clienteObterNome(o["cliente_id"].asString());
        std::cout << std::setw(14) << o["numero"].asString()
                  << std::setw(22) << cn.substr(0,21)
                  << std::setw(12) << o["estado"].asString()
                  << std::setw(12) << (std::to_string((int)(o["total"].asDouble())) + " EUR")
                  << o["data"].asString().substr(0,10) << "\n";
    }
}

/* ================================================================
 * Editar estado do orçamento
 * ================================================================ */
void orcamentosEditar() {
    subtitulo("ATUALIZAR ORÇAMENTO");
    std::string num = lerString("  Número do orçamento: ");

    JsonValue orcs = jsonParseFile(FILE_ORCAMENTOS);
    if (!orcs.isArray()) return;

    for (auto& o : orcs.arr) {
        if (o["numero"].asString()==num) {
            std::cout << "  Estado atual: " << o["estado"].asString() << "\n";
            std::cout << "  1. Marcar como aprovado\n";
            std::cout << "  2. Marcar como rejeitado\n";
            std::cout << "  0. Cancelar\n";
            int op = lerInteiro("  Opção: ", 0, 2);
            if (op==0) return;
            if (op==1) o["estado"] = JsonValue(std::string("aprovado"));
            if (op==2) o["estado"] = JsonValue(std::string("rejeitado"));
            jsonSaveFile(FILE_ORCAMENTOS, orcs);
            logRegistar("editar_orcamento", "numero=" + num + " estado=" + o["estado"].asString());
            std::cout << "  [OK] Estado atualizado.\n";
            return;
        }
    }
    std::cout << "  [!] Orçamento não encontrado.\n";
}

/* ================================================================
 * Converter orçamento aprovado em fatura
 * ================================================================ */
void orcamentosConverterFatura() {
    subtitulo("CONVERTER ORÇAMENTO EM FATURA");
    std::string num = lerString("  Número do orçamento: ");

    JsonValue orcs = jsonParseFile(FILE_ORCAMENTOS);
    if (!orcs.isArray()) { std::cout << "  Sem orçamentos.\n"; return; }

    for (auto& o : orcs.arr) {
        if (o["numero"].asString()==num) {
            if (o["estado"].asString() != "aprovado") {
                std::cout << "  [!] Apenas orçamentos aprovados podem ser convertidos.\n";
                std::cout << "      Estado atual: " << o["estado"].asString() << "\n";
                return;
            }
            if (!o["fatura_id"].asString().empty()) {
                std::cout << "  [!] Já convertido em fatura.\n"; return;
            }

            if (!lerSimNao("  Converter em fatura?")) return;

            // Criar fatura a partir do orçamento
            int seq = vendasProximoNumero();
            std::string numero_fat = gerarNumeroFatura(seq);

            JsonObject venda;
            venda["id"]         = JsonValue(generateId("vnd_"));
            venda["numero"]     = JsonValue(numero_fat);
            venda["cliente_id"] = JsonValue(o["cliente_id"].asString());
            venda["itens"]      = o["itens"];
            venda["total"]      = o["total"];
            venda["data"]       = JsonValue(dataAtual());
            venda["vendedor"]   = JsonValue(g_sessao.username);
            venda["orcamento"]  = JsonValue(num);

            JsonValue vendas = jsonParseFile(FILE_VENDAS);
            if (!vendas.isArray()) vendas = JsonValue(JsonArray{});
            vendas.arr.push_back(JsonValue(venda));
            jsonSaveFile(FILE_VENDAS, vendas);

            // Atualizar orçamento
            o["estado"]    = JsonValue(std::string("faturado"));
            o["fatura_id"] = JsonValue(numero_fat);
            jsonSaveFile(FILE_ORCAMENTOS, orcs);

            // Automações: reduzir stock + garantias
            std::vector<std::string> garantias_criadas;
            if (o["itens"].isArray()) {
                for (auto& item : o["itens"].arr) {
                    if (item["tipo"].asString()=="produto" || !item["produto_id"].asString().empty()) {
                        produtoReduzirStock(item["produto_id"].asString(), (int)item["quantidade"].asInt());
                    }
                    if (item["tem_garantia"].asBool()) {
                        std::string df = garantiaCriarAuto(
                            o["cliente_id"].asString(),
                            item["nome"].asString(),
                            (int)item["duracao_garantia"].asInt(),
                            numero_fat
                        );
                        garantias_criadas.push_back(item["nome"].asString() + " → " + df);
                    }
                }
            }

            logRegistar("converter_orcamento", "orc=" + num + " fat=" + numero_fat);

            std::cout << "\n  [OK] Fatura " << numero_fat << " criada a partir de " << num << "\n";
            if (!garantias_criadas.empty()) {
                std::cout << "  Garantias criadas:\n";
                for (auto& g : garantias_criadas) std::cout << "    - " << g << "\n";
                std::cout << "  Venda concluída. Garantias criadas automaticamente.\n";
            }

            std::string fp = docGerarFatura(JsonValue(venda));
            docApresentarOpcoes(fp, "Fatura");
            return;
        }
    }
    std::cout << "  [!] Orçamento não encontrado.\n";
}

/* ================================================================
 * Menu orçamentos
 * ================================================================ */
void orcamentosMenu() {
    while (true) {
        titulo("GESTÃO DE ORÇAMENTOS");
        std::cout << "  1. Criar orçamento\n";
        std::cout << "  2. Listar orçamentos\n";
        std::cout << "  3. Atualizar estado\n";
        std::cout << "  4. Converter em fatura\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 4);
        if (op==0) break;
        else if (op==1) orcamentosCriar();
        else if (op==2) orcamentosListar();
        else if (op==3) orcamentosEditar();
        else if (op==4) orcamentosConverterFatura();
        pausar();
    }
}
