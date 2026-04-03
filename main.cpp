/*
 * main.cpp - Ponto de entrada + menu principal TUI de ecra fixo
 *
 * Compilar Linux/macOS:
 *   g++ -std=c++11 -O2 -o gestao main.cpp auth.cpp clientes.cpp produtos.cpp \
 *       vendas.cpp orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
 *       logs.cpp documentos.cpp
 *
 * Compilar Windows (MinGW):
 *   g++ -std=c++11 -O2 -o gestao.exe main.cpp auth.cpp clientes.cpp produtos.cpp \
 *       vendas.cpp orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
 *       logs.cpp documentos.cpp
 */

#include <iostream>
#include <iomanip>
#include <cstdlib>

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

/* Globais */
Sessao      g_sessao;
std::string g_breadcrumb = "Menu Principal";
std::string g_dica       = "";

static void inicializarPastas() {
    MKDIR(DATA_DIR); MKDIR(DOCS_DIR);
}

/* ================================================================
 * Dashboard
 * ================================================================ */
static void dashboard() {
    iniciarEcra("DASHBOARD","Menu Principal > Dashboard","0=Voltar  Enter=OK");

    JsonValue clientes=jsonParseFile(FILE_CLIENTES);
    JsonValue produtos=jsonParseFile(FILE_PRODUTOS);
    JsonValue vendas  =jsonParseFile(FILE_VENDAS);
    JsonValue reps    =jsonParseFile(FILE_REPARACOES);
    JsonValue orcs    =jsonParseFile(FILE_ORCAMENTOS);

    int n_cli =(clientes.isArray())?(int)clientes.arr.size():0;
    int n_prd =(produtos.isArray())?(int)produtos.arr.size():0;
    int n_vnd =(vendas.isArray())  ?(int)vendas.arr.size()  :0;
    int n_rep =(reps.isArray())    ?(int)reps.arr.size()    :0;

    int reps_curso=0;
    if(reps.isArray())
        for(size_t i=0;i<reps.arr.size();i++){
            std::string e=reps.arr[i]["estado"].asString();
            if(e!="entregue"&&e!="concluido")++reps_curso;
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

    /* Caixas de metricas 2x3 */
    auto metricaLinha=[](const std::string& lbl, const std::string& val, const std::string& lbl2, const std::string& val2){
        /* cada caixa tem 34 colunas de largura interior */
        std::string c1=" \xe2\x94\x8c"+std::string(32,'\xe2')+std::string(1,'\x80')+std::string(1,'\x90')+" ";
        /* usar ASCII simples para compatibilidade */
        std::string row1=" +"+fw("",32,'-')+"+  +"+fw("",32,'-')+"+ ";
        std::string row2=" | "+fw(lbl,30)+" |  | "+fw(lbl2,30)+" | ";
        std::ostringstream v1,v2;
        v1<<std::setw(28)<<val; v2<<std::setw(28)<<val2;
        std::string row3=" | "+fw("",4)+"  "+v1.str()+" |  | "+fw("",4)+"  "+v2.str()+" | ";
        std::string row4=" +"+fw("",32,'-')+"+  +"+fw("",32,'-')+"+ ";
        std::cout<<boxL(row1);
        std::cout<<boxL(row2);
        std::cout<<boxL(row3);
        std::cout<<boxL(row4);
    };

    ecraVazia();

    /* linha 1 */
    std::cout<<boxL(" +--------------------------------+  +--------------------------------+");
    std::cout<<boxL(" |  Clientes registados           |  |  Produtos no catalogo          |");
    std::ostringstream s1,s2;
    s1<<std::setw(30)<<n_cli; s2<<std::setw(30)<<n_prd;
    std::cout<<boxL(" |"+s1.str()+"  |  |"+s2.str()+"  |");
    std::cout<<boxL(" +--------------------------------+  +--------------------------------+");
    ecraVazia();
    /* linha 2 */
    std::cout<<boxL(" +--------------------------------+  +--------------------------------+");
    std::cout<<boxL(" |  Vendas totais                 |  |  Vendas hoje (EUR)             |");
    std::ostringstream s3,s4; s3<<std::setw(30)<<n_vnd;
    s4<<std::fixed<<std::setprecision(2)<<std::setw(30)<<total_dia;
    std::cout<<boxL(" |"+s3.str()+"  |  |"+s4.str()+"  |");
    std::cout<<boxL(" +--------------------------------+  +--------------------------------+");
    ecraVazia();
    /* linha 3 */
    std::cout<<boxL(" +--------------------------------+  +--------------------------------+");
    std::cout<<boxL(" |  Reparacoes em curso           |  |  Orcamentos pendentes          |");
    std::string rep_str=std::to_string(reps_curso)+" / "+std::to_string(n_rep);
    std::ostringstream s5,s6;
    s5<<std::setw(30)<<rep_str; s6<<std::setw(30)<<orcs_pend;
    std::cout<<boxL(" |"+s5.str()+"  |  |"+s6.str()+"  |");
    std::cout<<boxL(" +--------------------------------+  +--------------------------------+");

    if(stock_baixo>0){
        ecraVazia();
        ecraErr(std::to_string(stock_baixo)+" produto(s) com stock abaixo do minimo!");
    }

    pausar();
}

/* ================================================================
 * Menu principal
 * ================================================================ */
static void menuPrincipal() {
    while(true) {
        iniciarEcra("MENU PRINCIPAL","Menu Principal","Introduza o numero da opcao");

        ecraLinha("  +--------------------------------------+");
        ecraLinha("  |   GESTAO DE LOJA DE ELECTRONICA      |");
        ecraLinha("  +--------------------------------------+");
        ecraVazia();
        ecraLinha("   1.  Dashboard");
        ecraLinha("   2.  Clientes");
        ecraLinha("   3.  Produtos");
        ecraLinha("   4.  Vendas / Faturas");
        ecraLinha("   5.  Orcamentos");
        ecraLinha("   6.  Reparacoes");
        ecraLinha("   7.  Garantias");
        if(temPermissao("admin")){
        ecraLinha("   8.  Lojas");
        ecraLinha("   9.  Utilizadores");
        }
        if(temPermissao("gerente")){
        ecraLinha("  10.  Logs do sistema");
        }
        ecraVazia();
        ecraLinha("   0.  Sair");

        fecharEcra();
        int op=lerInteiro("Opcao: ",0,10);

        switch(op){
            case 0:
                iniciarEcra("SAIR","Menu Principal > Sair","s=Sair  n=Voltar");
                fecharEcra();
                if(lerSimNao("Terminar sessao?")){
                    authLogout();
                    return;
                }
                break;
            case 1:  dashboard();       break;
            case 2:  clientesMenu();    break;
            case 3:  produtosMenu();    break;
            case 4:  vendasMenu();      break;
            case 5:  orcamentosMenu();  break;
            case 6:  reparacoesMenu();  break;
            case 7:  garantiasMenu();   break;
            case 8:
                if(temPermissao("admin")) lojasMenu();
                else erroPermissao();
                break;
            case 9:
                if(temPermissao("admin")) utilizadoresMenu();
                else erroPermissao();
                break;
            case 10:
                if(temPermissao("gerente")){ logsListar(); pausar(); }
                else erroPermissao();
                break;
        }
    }
}

/* ================================================================
 * Ecra de login
 * ================================================================ */
static bool ecrLogin() {
    iniciarEcra("LOGIN","","Introduza as credenciais");
    ecraVazia();
    ecraLinha("  +--------------------------------------+");
    ecraLinha("  |      TECHFIX - SISTEMA DE GESTAO     |");
    ecraLinha("  |      Loja de Reparacao e Venda        |");
    ecraLinha("  +--------------------------------------+");
    ecraVazia();
    fecharEcra();
    return authLogin();
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(){
    std::setlocale(LC_ALL,"");
    uiInitConsole();
    inicializarPastas();
    authInicializar();

    for(int i=0;i<3;++i){
        if(ecrLogin()){
            menuPrincipal();
            break;
        }
        if(i<2){
            msgErr("Credenciais invalidas. Tente novamente.");
            pausar("Prima Enter...");
        } else {
            msgErr("Maximo de tentativas atingido.");
            pausar("Prima Enter para sair...");
        }
    }

    cls();
    std::cout<<"\n  Sistema encerrado. Ate breve!\n\n";
    return 0;
}
