#include "relatorios.h"
#include "common.h"
#include "json_utils.h"
#include <iostream>
#include <map>
#include <algorithm>

static void vendasPorPeriodo() {
    std::string d1 = lerString("  Data inicio (YYYY-MM-DD): ");
    std::string d2 = lerString("  Data fim (YYYY-MM-DD): ");
    if (d1.empty()||d2.empty()) return;
    JsonValue vendas = jsonParseFile(FILE_VENDAS);
    double total=0; int count=0;
    if (vendas.isArray()) {
        for (auto &v : vendas.arr) {
            std::string d = v["data"].asString().substr(0,10);
            if (d>=d1 && d<=d2) { total += v["total"].asDouble(); ++count; }
        }
    }
    std::cout << "  Vendas entre "<<d1<<" e "<<d2<<": "<<count<<" vendas - Total: "<<total<<" EUR\n";
}

static void topProdutos() {
    JsonValue vendas = jsonParseFile(FILE_VENDAS);
    std::map<std::string,int> cnt;
    if (vendas.isArray()) {
        for (auto &v : vendas.arr) if (v["itens"].isArray()) {
            for (auto &i : v["itens"].arr) cnt[i["produto_id"].asString()] += (int)i["quantidade"].asInt();
        }
    }
    std::vector<std::pair<int,std::string>> vec;
    for (auto &p : cnt) vec.push_back({p.second, p.first});
    std::sort(vec.begin(), vec.end(), [](const std::pair<int,std::string>& a, const std::pair<int,std::string>& b){ return a.first > b.first; });
    std::cout << "  Top produtos (por quantidade):\n";
    for (size_t i=0;i<vec.size()&&i<10;++i) std::cout << "   "<<i+1<<" - "<<vec[i].second<<" x"<<vec[i].first<<"\n";
}

void relatoriosMenu() {
    while (true) {
        std::cout << "\n--- Relatórios ---\n";
        std::cout << "1) Vendas por período\n";
        std::cout << "2) Top produtos\n";
        std::cout << "0) Voltar\n";
        std::cout << "Opcao: ";
        std::string o; std::getline(std::cin,o);
        if (o.empty()) continue;
        if (o[0]=='1') vendasPorPeriodo();
        else if (o[0]=='2') topProdutos();
        else if (o[0]=='0') break;
    }
}
