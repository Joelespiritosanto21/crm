#include "caixa.h"
#include "common.h"
#include "json_utils.h"
#include "logs.h"

#include <iostream>
#include <sstream>

static const std::string FILE_CAIXA = std::string(DATA_DIR) + "caixa.json";

bool caixaAbrir(double fundo_inicial) {
    JsonValue caixas = jsonParseFile(FILE_CAIXA);
    if (!caixas.isArray()) caixas = JsonValue(JsonArray{});
    JsonObject sessao;
    sessao["id"] = JsonValue(generateId("cx_"));
    sessao["loja_id"] = JsonValue(g_sessao.loja_id);
    sessao["aberto_em"] = JsonValue(dataAtual());
    sessao["fechado_em"] = JsonValue();
    sessao["fundo_inicial"] = JsonValue(fundo_inicial);
    sessao["totais"] = JsonValue(JsonObject{});
    caixas.arr.push_back(JsonValue(sessao));
    bool ok = jsonSaveFile(FILE_CAIXA, caixas);
    if (ok) logRegistar("caixa_abrir", "loja=" + g_sessao.loja_id);
    return ok;
}

bool caixaFechar() {
    JsonValue caixas = jsonParseFile(FILE_CAIXA);
    if (!caixas.isArray() || caixas.arr.empty()) return false;
    // encontrar ultima sessao aberta para esta loja
    for (int i = (int)caixas.arr.size()-1; i>=0; --i) {
        JsonValue& s = caixas.arr[i];
        if (s["loja_id"].asString() == g_sessao.loja_id && s["fechado_em"].isNull()) {
            s["fechado_em"] = JsonValue(dataAtual());
            bool ok = jsonSaveFile(FILE_CAIXA, caixas);
            if (ok) logRegistar("caixa_fechar", "loja=" + g_sessao.loja_id);
            return ok;
        }
    }
    return false;
}

bool caixaRegistarPagamento(const std::string& metodo, double valor) {
    JsonValue caixas = jsonParseFile(FILE_CAIXA);
    if (!caixas.isArray() || caixas.arr.empty()) return false;
    for (int i = (int)caixas.arr.size()-1; i>=0; --i) {
        JsonValue& s = caixas.arr[i];
        if (s["loja_id"].asString() == g_sessao.loja_id && s["fechado_em"].isNull()) {
            // atualizar totais
            if (!s["totais"].isObject()) s["totais"] = JsonValue(JsonObject{});
            JsonValue& tot = s["totais"];
            double atual = tot[metodo].asDouble();
            tot[metodo] = JsonValue(atual + valor);
            bool ok = jsonSaveFile(FILE_CAIXA, caixas);
            if (ok) logRegistar("caixa_pago", "metodo="+metodo+" valor="+std::to_string(valor));
            return ok;
        }
    }
    // se nao houver caixa aberta, falha
    return false;
}

void caixaMenu() {
    while (true) {
        std::cout << "\n--- Caixa diária ---\n";
        std::cout << "1) Abrir caixa\n";
        std::cout << "2) Fechar caixa\n";
        std::cout << "3) Ver estado\n";
        std::cout << "0) Voltar\n";
        std::cout << "Opcao: ";
        std::string o; std::getline(std::cin,o);
        if (o.empty()) continue;
        if (o[0]=='1') {
            double fundo = 0.0;
            std::cout << "Fundo inicial (EUR): "; std::string s; std::getline(std::cin,s);
            try { fundo = std::stod(s); } catch(...) { fundo = 0.0; }
            if (caixaAbrir(fundo)) std::cout << "Caixa aberta.\n"; else std::cout << "Falha ao abrir caixa.\n";
        } else if (o[0]=='2') {
            if (caixaFechar()) std::cout << "Caixa fechada.\n"; else std::cout << "Nenhuma caixa aberta.\n";
        } else if (o[0]=='3') {
            JsonValue caixas = jsonParseFile(FILE_CAIXA);
            if (!caixas.isArray() || caixas.arr.empty()) { std::cout << "Sem sessões de caixa.\n"; continue; }
            bool found=false;
            for (int i=(int)caixas.arr.size()-1;i>=0;--i) {
                auto &s = caixas.arr[i];
                if (s["loja_id"].asString()==g_sessao.loja_id && s["fechado_em"].isNull()) {
                    found=true;
                    std::cout << "Aberto em: " << s["aberto_em"].asString() << "\n";
                    std::cout << "Fundo inicial: " << s["fundo_inicial"].asDouble() << " EUR\n";
                    if (s["totais"].isObject()) {
                        std::cout << "Totais por metodo:\n";
                        for (auto &kv : s["totais"].obj) {
                            std::cout << "  " << kv.first << ": " << kv.second.asDouble() << " EUR\n";
                        }
                    }
                    break;
                }
            }
            if (!found) std::cout << "Nenhuma caixa aberta para esta loja.\n";
        } else if (o[0]=='0') break;
    }
}
