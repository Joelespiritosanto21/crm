/*
 * vendas.cpp - Implementação de vendas e faturas
 */

#include "vendas.h"
#include "clientes.h"
#include "produtos.h"
#include "garantias.h"
#include "documentos.h"
#include "logs.h"
#include "data_layer.h"
#include "sync_manager.h"
#include <iostream>
#include <iomanip>
#include <vector>

/* ================================================================
 * Próximo número de fatura
 * ================================================================ */
int vendasProximoNumero() {
    JsonValue vendas = dl_get_vendas_local();
    if (!vendas.isArray() || vendas.arr.empty()) return 1;
    int max = 0;
    for (auto& v : vendas.arr) {
        // numero ex: "FAT-000001" → extrair os 6 dígitos
        std::string num = v["numero"].asString();
        if (num.size() > 4) {
            try { int n = std::stoi(num.substr(4)); if (n>max) max=n; } catch(...) {}
        }
    }
    return max + 1;
}

/* ================================================================
 * Criar venda / fatura
 * ================================================================ */
void vendasCriar() {
    titulo("NOVA VENDA / FATURA");

    /* 1. Encontrar ou criar cliente */
    JsonValue cliente = clienteEncontrarOuCriar();
    if (cliente.isNull()) {
        std::cout << "  [!] Venda cancelada.\n"; return;
    }
    std::string cliente_id   = cliente["id"].asString();
    std::string cliente_nome = cliente["nome"].asString();
    std::cout << "\n  Cliente: " << cliente_nome << "\n";

    /* 2. Adicionar itens */
    JsonArray itens;
    double total = 0.0;

    while (true) {
        subtitulo("ADICIONAR ITEM");
        std::cout << "  1. Pesquisar por EAN\n";
        std::cout << "  2. Pesquisar por nome\n";
        std::cout << "  0. Finalizar venda\n";
        int op = lerInteiro("  Opção: ", 0, 2);

        if (op==0) break;

        JsonValue produto;

        if (op==1) {
            std::string ean = lerString("  EAN: ");
            produto = produtoEncontrarPorEAN(ean);
            if (produto.isNull()) { std::cout << "  [!] EAN não encontrado.\n"; continue; }
        } else {
            std::string nome = lerString("  Nome (parte): ");
            std::string nomeLow = nome;
            std::transform(nomeLow.begin(), nomeLow.end(), nomeLow.begin(), ::tolower);

            JsonValue prods = dl_get_produtos_local();
            std::vector<JsonValue*> encontrados;
            if (prods.isArray()) {
                for (auto& p : prods.arr) {
                    if (!p["ativo"].asBool()) continue;
                    std::string pn = p["nome"].asString();
                    std::transform(pn.begin(), pn.end(), pn.begin(), ::tolower);
                    if (pn.find(nomeLow)!=std::string::npos) encontrados.push_back(&p);
                }
            }
            if (encontrados.empty()) { std::cout << "  [!] Nenhum produto encontrado.\n"; continue; }
            if (encontrados.size()==1) {
                produto = *encontrados[0];
            } else {
                std::cout << "  Resultados:\n";
                for (size_t i=0; i<encontrados.size() && i<10; ++i) {
                    std::cout << "  " << (i+1) << ". " << (*encontrados[i])["nome"].asString()
                              << "  " << std::fixed << std::setprecision(2)
                              << (*encontrados[i])["preco"].asDouble() << " EUR\n";
                }
                int sel = lerInteiro("  Selecionar (0=cancelar): ", 0, (int)std::min(encontrados.size(),(size_t)10));
                if (sel==0) continue;
                produto = *encontrados[sel-1];
            }
        }

        if (!produto["ativo"].asBool()) { std::cout << "  [!] Produto inativo.\n"; continue; }

        std::cout << "  Produto: " << produto["nome"].asString() << "\n";
        std::cout << "  Preço: " << std::fixed << std::setprecision(2) << produto["preco"].asDouble() << " EUR\n";

        if (produto["tipo"].asString()=="produto") {
            std::cout << "  Stock disponível: " << produto["stock"].asInt() << "\n";
        }

        int qty = lerInteiro("  Quantidade: ", 1, 9999);

        // Verificar stock (apenas para produtos físicos)
        if (produto["tipo"].asString()=="produto") {
            if (produto["stock"].asInt() < qty) {
                std::cout << "  [!] Stock insuficiente (" << produto["stock"].asInt() << " disponíveis).\n";
                continue;
            }
        }

        // Permitir desconto
        double preco = produto["preco"].asDouble();
        if (lerSimNao("  Aplicar desconto?")) {
            double desc = lerDouble("  Desconto (%): ");
            if (desc > 0 && desc < 100) {
                preco = preco * (1.0 - desc/100.0);
                std::cout << "  Preço com desconto: " << std::fixed << std::setprecision(2) << preco << " EUR\n";
            }
        }

        JsonObject item;
        item["produto_id"]      = JsonValue(produto["id"].asString());
        item["nome"]            = JsonValue(produto["nome"].asString());
        item["ean"]             = JsonValue(produto["ean"].asString());
        item["tipo"]            = JsonValue(produto["tipo"].asString());
        item["quantidade"]      = JsonValue((long long)qty);
        item["preco_unitario"]  = JsonValue(preco);
        item["tem_garantia"]    = JsonValue(produto["tem_garantia"].asBool());
        item["duracao_garantia"]= JsonValue(produto["duracao_garantia"].asInt());
        item["subtotal"]        = JsonValue(preco * qty);

        itens.push_back(JsonValue(item));
        total += preco * qty;

        std::cout << "  [+] Adicionado: " << produto["nome"].asString()
                  << " x" << qty
                  << " = " << std::fixed << std::setprecision(2) << (preco*qty) << " EUR\n";
    }

    if (itens.empty()) {
        std::cout << "  Nenhum item adicionado. Venda cancelada.\n"; return;
    }

    /* 3. Resumo */
    subtitulo("RESUMO DA VENDA");
    std::cout << "  Cliente: " << cliente_nome << "\n";
    std::cout << "  Itens: " << itens.size() << "\n";
    std::cout << "  TOTAL: " << std::fixed << std::setprecision(2) << total << " EUR\n\n";

    if (!lerSimNao("  Confirmar venda?")) {
        std::cout << "  Venda cancelada.\n"; return;
    }

    /* 4. Criar registo de venda */
    int seq = vendasProximoNumero();
    std::string numero = gerarNumeroFatura(seq);

    JsonObject venda;
    venda["id"]         = JsonValue(generateId("vnd_"));
    venda["numero"]     = JsonValue(numero);
    venda["cliente_id"] = JsonValue(cliente_id);
    venda["itens"]      = JsonValue(itens);
    venda["total"]      = JsonValue(total);
    venda["data"]       = JsonValue(dataAtual());
    venda["vendedor"]   = JsonValue(g_sessao.username);

    // Persistir venda localmente
    dl_add_venda_local(JsonValue(venda));
    // Enfileirar para sincronização com servidor
    sync_add_operation("create_venda", JsonValue(venda));

    /* 5. AUTOMAÇÕES PÓS-VENDA */
    std::vector<std::string> garantias_criadas;

    for (auto& item : itens) {
        // 5a. Reduzir stock
        if (item["tipo"].asString()=="produto") {
            produtoReduzirStock(item["produto_id"].asString(), (int)item["quantidade"].asInt());
        }

        // 5b. Criar garantia automática
        if (item["tem_garantia"].asBool()) {
            int dias = (int)item["duracao_garantia"].asInt();
            std::string data_fim = garantiaCriarAuto(
                cliente_id,
                item["nome"].asString(),
                dias,
                numero
            );
            garantias_criadas.push_back(item["nome"].asString() + " → " + data_fim);
        }
    }

    logRegistar("venda", "numero=" + numero + " cliente=" + cliente_nome + " total=" + std::to_string(total));

    /* 6. Mensagem de confirmação */
    std::cout << "\n  ====================================\n";
    std::cout << "  [OK] Venda concluída: " << numero << "\n";
    std::cout << "  Total: " << std::fixed << std::setprecision(2) << total << " EUR\n";
    if (!garantias_criadas.empty()) {
        std::cout << "\n  Garantias criadas automaticamente:\n";
        for (auto& g : garantias_criadas)
            std::cout << "    - " << g << "\n";
        std::cout << "\n  Venda concluída. Garantias criadas automaticamente.\n";
    }
    std::cout << "  ====================================\n";

    /* 7. Gerar documento */
    std::string filepath = docGerarFatura(JsonValue(venda));
    docApresentarOpcoes(filepath, "Fatura");
}

/* ================================================================
 * Listar vendas
 * ================================================================ */
void vendasListar() {
    JsonValue vendas = dl_get_vendas_local();
    subtitulo("LISTA DE VENDAS");

    if (!vendas.isArray() || vendas.arr.empty()) {
        std::cout << "  Sem vendas registadas.\n"; return;
    }

    std::cout << std::left
              << std::setw(14) << "NÚMERO"
              << std::setw(22) << "CLIENTE"
              << std::setw(20) << "DATA"
              << std::setw(12) << "TOTAL"
              << "VENDEDOR\n";
    linha();

    double total_geral = 0.0;
    for (auto& v : vendas.arr) {
        std::string cn = clienteObterNome(v["cliente_id"].asString());
        double t = v["total"].asDouble();
        total_geral += t;
        std::cout << std::setw(14) << v["numero"].asString()
                  << std::setw(22) << cn.substr(0,21)
                  << std::setw(20) << v["data"].asString().substr(0,19)
                  << std::setw(12) << (std::to_string((int)(t*100)/100) + " EUR")
                  << v["vendedor"].asString() << "\n";
    }
    linha();
    std::cout << "  Total geral: " << std::fixed << std::setprecision(2) << total_geral << " EUR"
              << "  (" << vendas.arr.size() << " vendas)\n";
}

/* ================================================================
 * Detalhe de uma venda
 * ================================================================ */
void vendasDetalhe() {
    subtitulo("DETALHE DE VENDA");
    std::string num = lerString("  Número da fatura (ex: FAT-000001): ");

    JsonValue vendas = dl_get_vendas_local();
    if (!vendas.isArray()) return;

    for (auto& v : vendas.arr) {
        if (v["numero"].asString()==num) {
            std::cout << "\n  Fatura: " << v["numero"].asString() << "\n";
            std::cout << "  Data: "    << v["data"].asString() << "\n";
            std::cout << "  Cliente: " << clienteObterNome(v["cliente_id"].asString()) << "\n";
            std::cout << "  Vendedor: "<< v["vendedor"].asString() << "\n\n";
            std::cout << std::left << std::setw(30) << "ITEM"
                      << std::setw(6) << "QTD"
                      << std::setw(12) << "P.UNIT"
                      << "SUBTOTAL\n";
            linha();
            if (v["itens"].isArray()) {
                for (auto& i : v["itens"].arr) {
                    std::cout << std::setw(30) << i["nome"].asString().substr(0,29)
                              << std::setw(6)  << i["quantidade"].asInt()
                              << std::setw(12) << (std::to_string((int)(i["preco_unitario"].asDouble()*100)/100.0)+"E")
                              << std::fixed << std::setprecision(2)
                              << i["subtotal"].asDouble() << " EUR\n";
                }
            }
            linha();
            std::cout << "  TOTAL: " << std::fixed << std::setprecision(2) << v["total"].asDouble() << " EUR\n";

            // Reimprimir documento?
            if (lerSimNao("\n  Reimprimir documento?")) {
                std::string fp = docGerarFatura(v);
                docApresentarOpcoes(fp, "Fatura");
            }
            return;
        }
    }
    std::cout << "  [!] Fatura não encontrada.\n";
}

/* ================================================================
 * Menu vendas
 * ================================================================ */
void vendasMenu() {
    while (true) {
        titulo("VENDAS / FATURAS");
        std::cout << "  1. Nova venda\n";
        std::cout << "  2. Listar vendas\n";
        std::cout << "  3. Detalhe de venda\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 3);
        if (op==0) break;
        else if (op==1) vendasCriar();
        else if (op==2) vendasListar();
        else if (op==3) vendasDetalhe();
        pausar();
    }
}
