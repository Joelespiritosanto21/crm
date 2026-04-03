/*
 * produtos.cpp - Implementação de gestão de produtos
 */

#include "produtos.h"
#include "logs.h"
#include "auth.h"
#include "data_layer.h"
#include "sync_manager.h"
#include <iostream>
#include "pedidos.h"
#include <iomanip>
#include <algorithm>

/* ================================================================
 * Encontrar produto por EAN
 * ================================================================ */
JsonValue produtoEncontrarPorEAN(const std::string& ean) {
    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) return JsonValue();
    for (auto& p : prods.arr)
        if (p["ean"].asString()==ean) return p;
    return JsonValue();
}

JsonValue produtoEncontrarPorId(const std::string& id) {
    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) return JsonValue();
    for (auto& p : prods.arr)
        if (p["id"].asString()==id) return p;
    return JsonValue();
}

/* ================================================================
 * Reduzir stock — retorna false se stock insuficiente
 * ================================================================ */
bool produtoReduzirStock(const std::string& produto_id, int quantidade) {
    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) return false;
    for (auto& p : prods.arr) {
        if (p["id"].asString()==produto_id) {
            int stock_atual = (int)p["stock"].asInt();
            if (stock_atual < quantidade) return false;
            p["stock"] = JsonValue((long long)(stock_atual - quantidade));
            dl_save_produtos_local(prods);
            return true;
        }
    }
    return false;
}

void produtosAtualizarStock(const std::string& produto_id, int delta) {
    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) return;
    for (auto& p : prods.arr) {
        if (p["id"].asString()==produto_id) {
            int novo = (int)p["stock"].asInt() + delta;
            if (novo < 0) novo = 0;
            p["stock"] = JsonValue((long long)novo);
            dl_save_produtos_local(prods);
            return;
        }
    }
}

/* ================================================================
 * Criar produto
 * ================================================================ */
void produtosCriar() {
    if (!temPermissao("gerente")) { erroPermissao(); return; }
    subtitulo("CRIAR PRODUTO / SERVIÇO");

    std::string tipo;
    std::cout << "  Tipo: 1=Produto  2=Serviço\n";
    int t = lerInteiro("  Opção: ", 1, 2);
    tipo = (t==1) ? "produto" : "servico";

    std::string nome = lerString("  Nome: ");
    if (nome.empty()) { std::cout << "  Nome obrigatório.\n"; return; }

    std::string descricao = lerString("  Descrição: ");

    double preco = lerDouble("  Preço (EUR): ");
    double preco_custo = 0.0;
    if (lerSimNao("  Inserir preço de custo?")) {
        preco_custo = lerDouble("  Preço de custo (EUR): ");
    }

    int stock = 0;
    if (tipo=="produto") {
        stock = lerInteiro("  Stock inicial: ", 0, 999999);
    }

    std::string ean;
    if (tipo=="produto") {
        while (true) {
            ean = lerString("  EAN (código de barras): ");
            if (ean.empty()) break;
            // Verificar duplicado
            JsonValue prods = dl_get_produtos_local();
            bool dup = false;
            if (prods.isArray()) for (auto& p : prods.arr) if (p["ean"].asString()==ean) { dup=true; break; }
            if (dup) { std::cout << "  [!] EAN já existe.\n"; continue; }
            break;
        }
    }

    bool tem_garantia = false;
    int duracao_garantia = 0;
    if (lerSimNao("  Tem garantia?")) {
        tem_garantia = true;
        duracao_garantia = lerInteiro("  Duração da garantia (dias): ", 1, 9999);
    }

    int stock_minimo = 0;
    if (tipo=="produto") {
        stock_minimo = lerInteiro("  Stock mínimo para alerta: ", 0, 9999);
    }

    JsonObject novo;
    novo["id"]               = JsonValue(generateId("prd_"));
    novo["nome"]             = JsonValue(nome);
    novo["descricao"]        = JsonValue(descricao);
    novo["preco"]            = JsonValue(preco);
    novo["preco_custo"]      = JsonValue(preco_custo);
    novo["stock"]            = JsonValue((long long)stock);
    novo["stock_minimo"]     = JsonValue((long long)stock_minimo);
    novo["ean"]              = JsonValue(ean);
    novo["tipo"]             = JsonValue(tipo);
    novo["tem_garantia"]     = JsonValue(tem_garantia);
    novo["duracao_garantia"] = JsonValue((long long)duracao_garantia);
    novo["ativo"]            = JsonValue(true);
    novo["criado_em"]        = JsonValue(dataAtual());

    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) prods = JsonValue(JsonArray{});
    prods.arr.push_back(JsonValue(novo));
    dl_save_produtos_local(prods);
    // Enfileirar criacao de produto para sincronizacao central
    sync_add_operation("create_produto", JsonValue(novo));

    logRegistar("criar_produto", "nome=" + nome + " ean=" + ean);
    std::cout << "\n  [OK] '" << nome << "' adicionado ao catálogo.\n";
}

/* ================================================================
 * Listar produtos
 * ================================================================ */
void produtosListar() {
    JsonValue prods = dl_get_produtos_local();
    subtitulo("CATÁLOGO DE PRODUTOS");
    if (!prods.isArray() || prods.arr.empty()) { std::cout << "  Sem produtos.\n"; return; }

    std::cout << "  Filtrar por: 1=Todos  2=Produto  3=Serviço  4=Stock baixo\n";
    int filtro = lerInteiro("  Opção: ", 1, 4);

    std::cout << "\n" << std::left
              << std::setw(28) << "NOME"
              << std::setw(10) << "TIPO"
              << std::setw(10) << "PREÇO"
              << std::setw(8)  << "STOCK"
              << std::setw(14) << "EAN"
              << "GARANTIA\n";
    linha();

    int count=0;
    for (auto& p : prods.arr) {
        if (!p["ativo"].asBool()) continue;
        std::string tipo = p["tipo"].asString();
        if (filtro==2 && tipo!="produto") continue;
        if (filtro==3 && tipo!="servico") continue;
        int stk = (int)p["stock"].asInt();
        int min = (int)p["stock_minimo"].asInt();
        if (filtro==4 && (tipo!="produto" || stk > min)) continue;

        std::string alerta = (tipo=="produto" && stk<=min) ? " [!]" : "";
        std::string garantia_str = p["tem_garantia"].asBool() ?
            std::to_string((int)p["duracao_garantia"].asInt()) + "d" : "---";

        double preco_val = p["preco"].asDouble();
        double custo_val = p.has("preco_custo") ? p["preco_custo"].asDouble() : 0.0;
        double margem_pc = (preco_val>0.0) ? ((preco_val - custo_val)/preco_val*100.0) : 0.0;
        std::ostringstream preco_s; preco_s<<std::fixed<<std::setprecision(2)<<preco_val<<"EUR";
        std::ostringstream margem_s; margem_s<<std::fixed<<std::setprecision(0)<<margem_pc<<"%";
        std::cout << std::setw(28) << p["nome"].asString().substr(0,27)
              << std::setw(10) << tipo
              << std::setw(10) << preco_s.str() << "(" << margem_s.str() << ")"
              << std::setw(8)  << (tipo=="produto" ? std::to_string(stk)+alerta : "---")
              << std::setw(14) << p["ean"].asString()
              << garantia_str << "\n";
        ++count;
    }
    std::cout << "\n  " << count << " produto(s).\n";
}

/* ================================================================
 * Editar produto
 * ================================================================ */
void produtosEditar() {
    if (!temPermissao("gerente")) { erroPermissao(); return; }
    subtitulo("EDITAR PRODUTO");

    std::string busca = lerString("  EAN ou nome: ");
    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) return;

    for (auto& p : prods.arr) {
        std::string nomeLow = p["nome"].asString();
        std::transform(nomeLow.begin(), nomeLow.end(), nomeLow.begin(), ::tolower);
        std::string buscaLow = busca;
        std::transform(buscaLow.begin(), buscaLow.end(), buscaLow.begin(), ::tolower);

        if (p["ean"].asString()==busca || nomeLow.find(buscaLow)!=std::string::npos) {
            std::cout << "  Produto: " << p["nome"].asString() << "\n";
            std::cout << "  1. Nome\n  2. Descrição\n  3. Preço\n  4. Stock\n";
            std::cout << "  5. Stock mínimo\n  6. Garantia\n  7. Desativar\n  0. Cancelar\n";
            int op = lerInteiro("  Campo: ", 0, 7);

            if (op==0) return;
            if (op==1) p["nome"]        = JsonValue(lerString("  Novo nome: "));
            if (op==2) p["descricao"]   = JsonValue(lerString("  Nova descrição: "));
            if (op==3) p["preco"]       = JsonValue(lerDouble("  Novo preço: "));
            if (op==4) p["stock"]       = JsonValue((long long)lerInteiro("  Novo stock: ", 0));
            if (op==5) p["stock_minimo"]= JsonValue((long long)lerInteiro("  Stock mínimo: ", 0));
            if (op==6) {
                bool tg = lerSimNao("  Tem garantia?");
                p["tem_garantia"] = JsonValue(tg);
                if (tg) p["duracao_garantia"] = JsonValue((long long)lerInteiro("  Dias de garantia: ", 1));
            }
            if (op==7) {
                if (!lerSimNao("  Desativar produto?")) return;
                p["ativo"] = JsonValue(false);
            }

            dl_save_produtos_local(prods);
            logRegistar("editar_produto", "id=" + p["id"].asString());
            std::cout << "  [OK] Produto atualizado.\n";
            return;
        }
    }
    std::cout << "  [!] Produto não encontrado.\n";
}

/* ================================================================
 * Apagar produto (desativar)
 * ================================================================ */
void produtosApagar() {
    if (!temPermissao("admin")) { erroPermissao(); return; }
    subtitulo("DESATIVAR PRODUTO");
    if (!authReautenticar()) return;

    std::string ean = lerString("  EAN do produto: ");
    JsonValue prods = dl_get_produtos_local();
    if (!prods.isArray()) return;
    for (auto& p : prods.arr) {
        if (p["ean"].asString()==ean) {
            p["ativo"] = JsonValue(false);
            dl_save_produtos_local(prods);
            logRegistar("desativar_produto", "ean=" + ean);
            std::cout << "  [OK] Produto desativado.\n";
            return;
        }
    }
    std::cout << "  [!] EAN não encontrado.\n";
}

/* ================================================================
 * Pesquisar por EAN
 * ================================================================ */
void produtosPesquisarEAN() {
    subtitulo("PESQUISAR POR EAN");
    std::string ean = lerString("  EAN: ");
    JsonValue p = produtoEncontrarPorEAN(ean);
    if (p.isNull()) { std::cout << "  [!] EAN não encontrado.\n"; return; }

    std::cout << "\n";
    std::cout << "  Nome:       " << p["nome"].asString() << "\n";
    std::cout << "  Descrição:  " << p["descricao"].asString() << "\n";
    std::cout << "  Tipo:       " << p["tipo"].asString() << "\n";
    std::cout << "  Preço:      " << std::fixed << std::setprecision(2) << p["preco"].asDouble() << " EUR\n";
    std::cout << "  Stock:      " << p["stock"].asInt() << "\n";
    std::cout << "  Garantia:   " << (p["tem_garantia"].asBool() ?
        std::to_string((int)p["duracao_garantia"].asInt())+"d" : "Não") << "\n";
}

/* ================================================================
 * Alertas de stock baixo
 * ================================================================ */
void produtosAlertasStock() {
    JsonValue prods = dl_get_produtos_local();
    subtitulo("ALERTAS DE STOCK BAIXO");
    if (!prods.isArray()) { std::cout << "  Sem dados.\n"; return; }

    int count = 0;
    for (auto& p : prods.arr) {
        if (!p["ativo"].asBool() || p["tipo"].asString()!="produto") continue;
        int stk = (int)p["stock"].asInt();
        int min = (int)p["stock_minimo"].asInt();
        if (stk <= min) {
            std::cout << "  [!] " << std::left << std::setw(30) << p["nome"].asString()
                      << " Stock: " << stk << " (mín: " << min << ")\n";
            ++count;
        }
    }
    if (!count) std::cout << "  Sem alertas de stock.\n";
    else std::cout << "\n  " << count << " produto(s) com stock baixo!\n";
    if (count>0) {
        if (lerSimNao("  Gerar encomenda para algum produto?")) {
            std::string pid = lerString("  ID do produto: ");
            int q = lerInteiro("  Quantidade a encomendar: ", 1, 999999);
            if (!pid.empty()) {
                // criar encomenda
                pedidosCriarParaProduto(pid, q);
                std::cout << "  Encomenda criada para produto " << pid << " (q=" << q << ").\n";
            }
        }
    }
}

/* ================================================================
 * Menu produtos
 * ================================================================ */
void produtosMenu() {
    while (true) {
        titulo("GESTÃO DE PRODUTOS");
        std::cout << "  1. Criar produto/serviço\n";
        std::cout << "  2. Listar catálogo\n";
        std::cout << "  3. Pesquisar por EAN\n";
        std::cout << "  4. Editar produto\n";
        std::cout << "  5. Desativar produto\n";
        std::cout << "  6. Alertas de stock\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 6);
        if (op==0) break;
        else if (op==1) produtosCriar();
        else if (op==2) produtosListar();
        else if (op==3) produtosPesquisarEAN();
        else if (op==4) produtosEditar();
        else if (op==5) produtosApagar();
        else if (op==6) produtosAlertasStock();
        pausar();
    }
}
