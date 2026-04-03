/*
 * main.cpp - Interface TUI estilo SSN/Radio Popular
 */

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p,0755)
#endif

#include "common.h"
#include "json_utils.h"
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
#include "caixa.h"
#include "devolucoes.h"
#include "transferencias.h"
#include "fornecedores.h"
#include "relatorios.h"
#include "pedidos.h"
#include "sync_manager.h"

/* ── Globais ─────────────────────────────────────────────── */
Sessao      g_sessao;
std::string g_breadcrumb = "MENU PRINCIPAL";
std::string g_dica       = "";
std::string g_loja_nome  = "TECHFIX";
/* Sync defaults */
std::string g_servidor_host = "localhost";
int         g_servidor_port = 2022;
std::string g_api_token     = "";

static void inicializarPastas() { MKDIR(DATA_DIR); MKDIR(DOCS_DIR); }

/* ================================================================
 * DASHBOARD estilo SSN
 * ================================================================ */
static void dashboard() {
    g_dica = "D-Dashboard  9-Sair";
    iniciarEcra("", "DASHBOARD", "");

    JsonValue clientes=jsonParseFile(FILE_CLIENTES);
    JsonValue produtos=jsonParseFile(FILE_PRODUTOS);
    JsonValue vendas  =jsonParseFile(FILE_VENDAS);
    JsonValue reps    =jsonParseFile(FILE_REPARACOES);
    JsonValue orcs    =jsonParseFile(FILE_ORCAMENTOS);

    int n_cli=(clientes.isArray())?(int)clientes.arr.size():0;
    int n_prd=(produtos.isArray())?(int)produtos.arr.size():0;
    int n_vnd=(vendas.isArray())  ?(int)vendas.arr.size()  :0;
    int n_rep=(reps.isArray())    ?(int)reps.arr.size()    :0;

    int reps_curso=0;
    if(reps.isArray())
        for(size_t i=0;i<reps.arr.size();i++){
            std::string e=reps.arr[i]["estado"].asString();
            if(e!="entregue"&&e!="concluido") ++reps_curso;
        }
    int orcs_pend=0;
    if(orcs.isArray())
        for(size_t i=0;i<orcs.arr.size();i++)
            if(orcs.arr[i]["estado"].asString()=="pendente") ++orcs_pend;

    double total_dia=0.0;
    std::string hoje=dataApenasData();
    if(vendas.isArray())
        for(size_t i=0;i<vendas.arr.size();i++)
            if(vendas.arr[i]["data"].asString().substr(0,10)==hoje)
                total_dia+=vendas.arr[i]["total"].asDouble();

    int stock_baixo=0;
    if(produtos.isArray())
        for(size_t i=0;i<produtos.arr.size();i++){
            JsonValue& p=produtos.arr[i];
            if(!p["ativo"].asBool()||p["tipo"].asString()!="produto") continue;
            if(p["stock"].asInt()<=p["stock_minimo"].asInt()) ++stock_baixo;
        }

    /* Cabecalho da tabela */
    std::cout << "\n";
    std::vector<std::pair<std::string,int>> cols = {
        {"INDICADOR",30}, {"VALOR",15}, {"  INDICADOR",28}, {"VALOR",8}
    };
    cabecalhoTabela(cols);

    auto row=[](const std::string& l, const std::string& v,
                const std::string& l2, const std::string& v2){
        std::cout << ANSI_WHITE
                  << fw(l,30) << fwR(v,14) << "  "
                  << fw(l2,26) << fwR(v2,8)
                  << ANSI_RESET << "\n";
    };

    std::ostringstream ss_total; ss_total<<std::fixed<<std::setprecision(2)<<total_dia;

    row("Clientes registados",  std::to_string(n_cli),
        "Vendas totais",        std::to_string(n_vnd));
    row("Produtos no catalogo", std::to_string(n_prd),
        "Vendas hoje (EUR)",    ss_total.str());
    row("Reparacoes em curso",  std::to_string(reps_curso)+" / "+std::to_string(n_rep),
        "Orcamentos pendentes", std::to_string(orcs_pend));

    if(stock_baixo>0) {
        hline('-');
        std::cout << ANSI_RED << ANSI_BOLD
                  << "  ATENCAO: " << stock_baixo
                  << " produto(s) com stock abaixo do minimo!"
                  << ANSI_RESET << "\n";
    }

    hline('-');
    pausar();
}

/* ================================================================
 * MENU PRINCIPAL estilo SSN
 *
 *  0-Venda a Publico        A-Clientes
 *  1-Produtos               B-Orcamentos
 *  2-Reparacoes             C-Garantias
 *  3-Dashboard              D-Lojas (admin)
 *                           E-Utilizadores (admin)
 *                           L-Logs
 *  9-Sair  F-Sair
 * ================================================================ */
static void menuPrincipal() {
    while(true) {
        g_dica = "SUB-MENU -------------------------------->";
        iniciarEcra("", "MENU PRINCIPAL", "");

        /* Cabecalho da area de artigos - igual ao SSN */
        std::cout << "\n";
        std::vector<std::pair<std::string,int>> cols = {
            {"ARTIGO",12}, {"QUANTID",10}, {"DESCRICAO",30}, {"T",3}, {"PRECO",10}, {"LJ",4}
        };
        cabecalhoTabela(cols);
        std::cout << "\n";

        /* Menu em duas colunas */
        std::vector<MenuItem> esq, dir;

        esq.push_back({"0","Venda a Publico / Nova Venda"});
        esq.push_back({"1","Clientes"});
        esq.push_back({"2","Produtos / Catalogo"});
        esq.push_back({"3","Reparacoes"});
        esq.push_back({"4","Orcamentos"});
        esq.push_back({"5","Garantias"});
        esq.push_back({"6","Dashboard / Resumo"});

        dir.push_back({"A","Pesquisar Cliente"});
        dir.push_back({"B","Nova Reparacao"});
        dir.push_back({"C","Verificar Garantia"});
        dir.push_back({"G","Alertas de Stock"});
        if(temPermissao("gerente")) dir.push_back({"L","Logs do Sistema"});
        if(temPermissao("admin"))   dir.push_back({"S","Lojas"});
        if(temPermissao("admin"))   dir.push_back({"U","Utilizadores"});
        dir.push_back({"Z","Sincronizar com servidor"});
        dir.push_back({"K","Caixa"});
        dir.push_back({"E","Devoluções"});
        dir.push_back({"T","Transferências"});
        dir.push_back({"O","Fornecedores"});
        dir.push_back({"R","Relatórios"});
        dir.push_back({"N","Encomendas"});

        std::cout << "\n";
        menuDuasColunas(esq, dir, 40);
        std::cout << "\n";
        menuItem("9","Sair"); std::cout << "   "; menuItem("F","Sair"); std::cout << "\n";
        std::cout << "\n";
        hline('-');
        /* Linha de SUB-TOTAL como no SSN */
        std::cout << ANSI_DIM << "                                        "
                  << "SUB-TOTAL ---------------->" << ANSI_RESET << "\n";

        char op = lerOpcaoMenu("Opcao: ");

        switch(op) {
            case '0': vendasMenu();         break;
            case '1': clientesMenu();       break;
            case '2': produtosMenu();       break;
            case '3': reparacoesMenu();     break;
            case '4': orcamentosMenu();     break;
            case '5': garantiasMenu();      break;
            case '6': dashboard();          break;
            case 'A': clientesPesquisar(); pausar(); break;
            case 'B': reparacoesCriar();             break;
            case 'C': garantiasVerificar(); pausar(); break;
            case 'G': produtosAlertasStock(); pausar(); break;
            case 'L':
                if(temPermissao("gerente")){ logsListar(); pausar(); }
                else erroPermissao();
                break;
            case 'Z':
                syncManagerMenu();
                break;
            case 'K':
                caixaMenu();
                break;
            case 'E':
                devolucoesMenu();
                break;
            case 'T':
                transferenciasMenu();
                break;
            case 'O':
                fornecedoresMenu();
                break;
            case 'R':
                relatoriosMenu();
                break;
            case 'N':
                pedidosMenu();
                break;
            case 'S':
                if(temPermissao("admin")) lojasMenu();
                else erroPermissao();
                break;
            case 'U':
                if(temPermissao("admin")) utilizadoresMenu();
                else erroPermissao();
                break;
            case '9':
            case 'F':
                g_dica = "S-Confirmar  N-Voltar";
                iniciarEcra("","SAIR","");
                std::cout << "\n";
                if(lerSimNao("Terminar sessao?")) {
                    authLogout();
                    return;
                }
                break;
            default:
                msgErr("Opcao invalida");
                break;
        }
    }
}

/* ================================================================
 * ECRA DE LOGIN estilo SSN
 * ================================================================ */
static bool ecrLogin() {
    cls();
    std::cout << ANSI_BG_BLACK;

    /* Cabecalho simplificado sem sessao */
    std::cout << ANSI_BOLD << ANSI_WHITE;
    std::cout << fw("TECHFIX - SISTEMA DE GESTAO",50)
              << fw("DATA : "+dataFormatada(),30) << "\n";
    std::cout << fw("Loja de Reparacao e Venda de Electronica",50)
              << fw(horaAtual(),30) << "\n";
    std::cout << ANSI_RESET;
    hline('=');

    std::cout << "\n\n";
    std::cout << ANSI_WHITE;
    std::cout << "  IDENTIFICACAO DO OPERADOR\n";
    hline('-',40);
    std::cout << ANSI_RESET;
    std::cout << "\n";

    return authLogin();
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main() {
    std::setlocale(LC_ALL,"");
    uiInitConsole();
    inicializarPastas();
    authInicializar();

    for(int i=0;i<3;++i) {
        if(ecrLogin()) {
            /* Carregar nome da loja */
            JsonValue lojas = jsonParseFile(FILE_LOJAS);
            if(lojas.isArray() && !lojas.arr.empty()) {
                g_sessao.loja_nome = lojas.arr[0]["nome"].asString();
                g_sessao.loja_id   = lojas.arr[0]["id"].asString();
            } else {
                g_sessao.loja_nome = "TECHFIX";
            }
            /* Carregar nome completo do utilizador */
            JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
            if(utils.isArray())
                for(size_t j=0;j<utils.arr.size();j++)
                    if(utils.arr[j]["username"].asString()==g_sessao.username)
                        g_sessao.nome = utils.arr[j]["nome"].asString();

            menuPrincipal();
            break;
        }
        if(i<2) {
            msgErr("Credenciais invalidas. Tente novamente.");
            pausar();
        } else {
            msgErr("Maximo de tentativas atingido.");
            pausar("Prima ENTER para sair...");
        }
    }

    cls();
    std::cout << "\n  Sistema encerrado. Ate breve!\n\n";
    return 0;
}
