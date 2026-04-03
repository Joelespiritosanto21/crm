/*
 * main.cpp - Ponto de entrada do sistema de gestão empresarial
 *
 * Sistema de Gestão para Loja de Reparação e Venda de Eletrónica
 * Compilar: g++ -std=c++17 -O2 -o gestao main.cpp auth.cpp clientes.cpp
 *           produtos.cpp vendas.cpp orcamentos.cpp reparacoes.cpp
 *           garantias.cpp lojas.cpp logs.cpp documentos.cpp
 * Executar: ./gestao
 */

#include <iostream>
#include <locale>
#include <iomanip>
#include <cstdlib>

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define MKDIR(p) mkdir(p, 0755)
#endif

#include "common.h"
#include "auth.h"
#include "clientes.h"
#include "produtos.h"
#include "vendas.h"
#include "orcamentos.h"
#include "reparacoes.h"
#include "garantias.h"
#include "lojas.h"
#include "logs.h"
#include "documentos.h"
#include "ui.h"
#include "server.h"

/* Sessão global */
Sessao g_sessao;

/* ================================================================
 * Criar estrutura de pastas necessárias
 * ================================================================ */
static void inicializarPastas() {
    MKDIR(DATA_DIR);
    MKDIR(DOCS_DIR);
}

/* ================================================================
 * Painel de informação da sessão
 * ================================================================ */
static void painelSessao() {
    std::cout << "\n";
    linha('=');
    std::cout << "  SISTEMA DE GESTÃO | Utilizador: " << g_sessao.username
              << " [" << g_sessao.role << "]"
              << "  | " << dataAtual() << "\n";
    linha('=');
}

/* ================================================================
 * Dashboard rápido
 * ================================================================ */
static void dashboard() {
    titulo("DASHBOARD");

    // Contar registos
    JsonValue clientes  = jsonParseFile(FILE_CLIENTES);
    JsonValue produtos  = jsonParseFile(FILE_PRODUTOS);
    JsonValue vendas    = jsonParseFile(FILE_VENDAS);
    JsonValue reps      = jsonParseFile(FILE_REPARACOES);
    JsonValue orcs      = jsonParseFile(FILE_ORCAMENTOS);

    int n_clientes = clientes.isArray() ? (int)clientes.arr.size() : 0;
    int n_produtos = produtos.isArray() ? (int)produtos.arr.size() : 0;
    int n_vendas   = vendas.isArray()   ? (int)vendas.arr.size()   : 0;
    int n_reps     = reps.isArray()     ? (int)reps.arr.size()     : 0;
    int n_orcs     = orcs.isArray()     ? (int)orcs.arr.size()     : 0;

    // Reparações em curso
    int reps_curso = 0;
    if (reps.isArray()) {
        for (auto& r : reps.arr) {
            std::string e = r["estado"].asString();
            if (e!="entregue" && e!="concluido") ++reps_curso;
        }
    }

    // Orçamentos pendentes
    int orcs_pend = 0;
    if (orcs.isArray()) {
        for (auto& o : orcs.arr) if (o["estado"].asString()=="pendente") ++orcs_pend;
    }

    // Total vendas do dia
    double total_dia = 0.0;
    std::string hoje = dataApenasData();
    if (vendas.isArray()) {
        for (auto& v : vendas.arr)
            if (v["data"].asString().substr(0,10)==hoje) total_dia += v["total"].asDouble();
    }

    // Stock baixo
    int stock_baixo = 0;
    if (produtos.isArray()) {
        for (auto& p : produtos.arr) {
            if (!p["ativo"].asBool() || p["tipo"].asString()!="produto") continue;
            if (p["stock"].asInt() <= p["stock_minimo"].asInt()) ++stock_baixo;
        }
    }

    std::cout << "\n";
    std::cout << "  Clientes registados   : " << n_clientes << "\n";
    std::cout << "  Produtos no catálogo  : " << n_produtos << "\n";
    std::cout << "  Total de vendas       : " << n_vendas   << "\n";
    std::cout << "  Vendas hoje           : " << std::fixed << std::setprecision(2) << total_dia << " EUR\n";
    std::cout << "  Reparações em curso   : " << reps_curso << " / " << n_reps << "\n";
    std::cout << "  Orçamentos pendentes  : " << orcs_pend  << "\n";
    if (stock_baixo > 0)
        std::cout << "\n  [!] " << stock_baixo << " produto(s) com stock baixo!\n";
}

/* ================================================================
 * Menu principal
 * ================================================================ */
static void menuPrincipal() {
    while (true) {
        painelSessao();

        std::cout << "\n";
        std::cout << "  1.  Dashboard\n";
        std::cout << "  2.  Clientes\n";
        std::cout << "  3.  Produtos\n";
        std::cout << "  4.  Vendas / Faturas\n";
        std::cout << "  5.  Orçamentos\n";
        std::cout << "  6.  Reparações\n";
        std::cout << "  7.  Garantias\n";

        if (temPermissao("admin")) {
            std::cout << "  8.  Lojas\n";
            std::cout << "  9.  Utilizadores\n";
        }
        if (temPermissao("gerente")) {
            std::cout << "  10. Logs do sistema\n";
        }
        std::cout << "  0.  Sair\n";
        std::cout << "\n";

        int op = lerInteiro("  Opção: ", 0, 10);

        switch(op) {
            case 0:
                if (lerSimNao("  Terminar sessão?")) {
                    authLogout();
                    return;
                }
                break;
            case 1: dashboard();   pausar(); break;
            case 2: clientesMenu();          break;
            case 3: produtosMenu();          break;
            case 4: vendasMenu();            break;
            case 5: orcamentosMenu();        break;
            case 6: reparacoesMenu();        break;
            case 7: garantiasMenu();         break;
            case 8:
                if (temPermissao("admin")) lojasMenu();
                else erroPermissao();
                break;
            case 9:
                if (temPermissao("admin")) utilizadoresMenu();
                else erroPermissao();
                break;
            case 10:
                if (temPermissao("gerente")) { logsListar(); pausar(); }
                else erroPermissao();
                break;
        }
    }
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main() {
    // Configurar locale para suporte a caracteres PT
    std::setlocale(LC_ALL, "");

    // Inicializar interface
    g_ui.inicializar();

    // Criar estrutura de pastas
    inicializarPastas();
    authInicializar();

    // Iniciar servidor web (background)
    std::cout << "\n";
    linha('=', LARGURA_JANELA);
    std::cout << "  SISTEMA DE GESTÃO — ELETRÓNICA\n";
    std::cout << "  Loja de Reparação e Venda de Equipamentos Eletrónicos\n";
    linha('=', LARGURA_JANELA);
    
    // Iniciar servidor web na porta 2021
    std::cout << "\n  [SERVIDOR] Iniciando interface web na porta 2021...\n";
    if (!iniciarServidorWeb()) {
        std::cout << "  [AVISO] Não foi possível iniciar servidor web\n";
    } else {
        std::cout << "  [OK] Servidor web ativo: http://localhost:2021\n";
    }

    std::cout << "\n";
    pausar();

    // Tentativas de login
    for (int i = 0; i < 3; ++i) {
        g_ui.limparEcraCompleto();
        g_ui.mostraTimeslot();
        
        if (authLogin()) {
            menuPrincipal();
            break;
        }
        if (i < 2) {
            std::cout << "  Tente novamente.\n\n";
            pausar();
        } else {
            std::cout << "  Máximo de tentativas atingido. A sair.\n";
            pausar();
        }
    }

    g_ui.limparEcraCompleto();
    std::cout << "\n  Sistema encerrado.\n\n";
    return 0;
}
