#include "devolucoes.h"
#include "common.h"
#include "json_utils.h"
#include "logs.h"
#include "vendas.h"
#include "produtos.h"

#include <iostream>

static const std::string FILE_DEVOLUCOES = std::string(DATA_DIR) + "devolucoes.json";

void devolucaoCriar() {
    std::string num = lerString("  Número da fatura (ex: FAT-000001): ");
    if (num.empty()) return;
    JsonValue vendas = jsonParseFile(FILE_VENDAS);
    if (!vendas.isArray()) { std::cout << "  Sem vendas."; return; }

    JsonValue venda;
    for (auto &v : vendas.arr) if (v["numero"].asString()==num) { venda = v; break; }
    if (venda.isNull()) { std::cout << "  Fatura não encontrada.\n"; return; }

    // Listar itens
    if (!venda["itens"].isArray()) { std::cout << "  Sem itens na fatura.\n"; return; }
    std::cout << "  Itens na fatura:\n";
    for (size_t i=0;i<venda["itens"].arr.size();++i) {
        auto &it = venda["itens"].arr[i];
        std::cout << "  " << (i+1) << ") " << it["nome"].asString() << " x" << it["quantidade"].asInt() << "\n";
    }
    int sel = lerInteiro("  Selecionar item a devolver (0=cancelar): ", 0, (int)venda["itens"].arr.size());
    if (sel==0) return;
    JsonValue item = venda["itens"].arr[sel-1];
    int maxq = (int)item["quantidade"].asInt();
    int q = lerInteiro("  Quantidade a devolver: ", 1, maxq);

    // Repor stock
    produtosAtualizarStock(item["produto_id"].asString(), q);

    // Registar devolução
    JsonValue devs = jsonParseFile(FILE_DEVOLUCOES);
    if (!devs.isArray()) devs = JsonValue(JsonArray{});
    JsonObject d;
    d["id"] = JsonValue(generateId("dv_"));
    d["venda_num"] = JsonValue(num);
    d["produto_id"] = JsonValue(item["produto_id"].asString());
    d["quantidade"] = JsonValue((long long)q);
    d["data"] = JsonValue(dataAtual());
    d["loja_id"] = JsonValue(g_sessao.loja_id);
    devs.arr.push_back(JsonValue(d));
    jsonSaveFile(FILE_DEVOLUCOES, devs);
    logRegistar("devolucao", "venda="+num+" prod="+item["produto_id"].asString()+" q="+std::to_string(q));

    std::cout << "  Devolução registada e stock atualizado.\n";
}

void devolucoesMenu() {
    while (true) {
        std::cout << "\n--- Devoluções ---\n";
        std::cout << "1) Registrar devolução\n";
        std::cout << "2) Listar devoluções\n";
        std::cout << "0) Voltar\n";
        std::cout << "Opcao: ";
        std::string o; std::getline(std::cin,o);
        if (o.empty()) continue;
        if (o[0]=='1') devolucaoCriar();
        else if (o[0]=='2') {
            JsonValue devs = jsonParseFile(FILE_DEVOLUCOES);
            if (!devs.isArray()||devs.arr.empty()) { std::cout<<"  Sem devoluções.\n"; continue; }
            for (auto &d : devs.arr) {
                std::cout << "  " << d["id"].asString() << " - " << d["venda_num"].asString()
                          << " - " << d["produto_id"].asString() << " x" << d["quantidade"].asInt()
                          << " - " << d["data"].asString() << "\n";
            }
        } else if (o[0]=='0') break;
    }
}
