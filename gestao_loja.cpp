/*
 * gestao_loja.cpp - Cliente TechFix para PCs das lojas
 *
 * Liga ao servidor central via TCP (porta 2022)
 * Sem ligacao = sem acesso (sem modo offline)
 *
 * Compilar:
 *   g++ -std=c++11 -O2 -o gestao_loja gestao_loja.cpp -lpthread
 *   (ou via: make gestao_loja)
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

#ifdef _WIN32
  #include <direct.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
#endif

#include "common.h"
#include "json_utils.h"
#include "sha256.h"
#include "net_utils.h"
#include "protocolo.h"
#include "client.h"

/* Globais (extern em common.h) */
Sessao      g_sessao;
std::string g_breadcrumb = "Menu Principal";
std::string g_dica       = "";
std::string g_loja_nome  = "TECHFIX";
ClientState g_client;

/* ================================================================
 * UTILITARIOS DE APRESENTACAO
 * ================================================================ */
static void mostrarErro(const JsonValue& r) {
    ecraErr(r["erro"].asString().empty() ? "Erro desconhecido" : r["erro"].asString());
}

static std::string promptCliente(const std::string& titulo) {
    /* Procurar ou criar cliente - retorna cliente_id */
    iniciarEcra("","",titulo);
    ecraVazia();
    ecraLinha("  1-Pesquisar por NIF");
    ecraLinha("  2-Pesquisar por Telefone");
    ecraLinha("  3-Pesquisar por Email");
    ecraLinha("  4-Pesquisar por Nome");
    ecraLinha("  5-Criar novo cliente");
    ecraLinha("  0-Cancelar");
    fecharEcra();

    char op = lerOpcaoMenu("Opcao: ");
    if (op=='0') return "";

    std::string campo, prompt_txt;
    if (op=='1') { campo="nif";      prompt_txt="NIF: "; }
    else if(op=='2'){ campo="telefone";prompt_txt="Telefone: "; }
    else if(op=='3'){ campo="email";   prompt_txt="Email: "; }
    else if(op=='4'){ campo="nome";    prompt_txt="Nome (parte): "; }
    else if(op=='5') {
        /* Criar novo cliente */
        iniciarEcra("","","CRIAR CLIENTE");
        JsonObject d;
        d["nome"]     = JsonValue(lerString("Nome completo: "));
        if(d["nome"].asString().empty()) return "";
        d["telefone"] = JsonValue(lerString("Telefone: "));
        d["email"]    = JsonValue(lerString("Email (opcional): "));
        d["nif"]      = JsonValue(lerString("NIF (opcional): "));
        JsonValue r = clientCmd(CMD_CLI_CRIAR, d);
        if (!clientOk(r)) { pausar(); return ""; }
        ecraOk("Cliente criado: " + r["data"]["nome"].asString());
        pausar();
        return r["data"]["id"].asString();
    }

    std::string valor = lerString(prompt_txt);
    if (valor.empty()) return "";

    JsonObject d;
    d["campo"] = JsonValue(campo);
    d["valor"] = JsonValue(valor);
    JsonValue r = clientCmd(CMD_CLI_BUSCAR, d);

    if (!r["ok"].asBool()) {
        ecraErr(r["erro"].asString());
        if (lerSimNao("Criar novo cliente?")) {
            /* Criar com o valor como pre-definicao */
            iniciarEcra("","","CRIAR CLIENTE");
            JsonObject nd;
            nd["nome"]    = JsonValue(lerString("Nome completo: "));
            nd["telefone"]= JsonValue(campo=="telefone"?valor:lerString("Telefone: "));
            nd["email"]   = JsonValue(campo=="email"?valor:lerString("Email (opcional): "));
            nd["nif"]     = JsonValue(campo=="nif"?valor:lerString("NIF (opcional): "));
            JsonValue cr = clientCmd(CMD_CLI_CRIAR, nd);
            if (!clientOk(cr)) { pausar(); return ""; }
            ecraOk("Cliente criado: "+cr["data"]["nome"].asString());
            pausar();
            return cr["data"]["id"].asString();
        }
        pausar(); return "";
    }

    /* Pode retornar um ou multiplos */
    if (r["data"].isArray()) {
        ecraVazia();
        ecraLinha("  Resultados encontrados:");
        ecraVazia();
        for (size_t i=0; i<r["data"].arr.size() && i<10; i++) {
            std::ostringstream o;
            o << "  " << (i+1) << ". " << fw(r["data"].arr[i]["nome"].asString(),25)
              << " NIF: " << fw(r["data"].arr[i]["nif"].asString(),12)
              << " Tel: " << r["data"].arr[i]["telefone"].asString();
            ecraLinha(o.str());
        }
        fecharEcra();
        int sel = lerInteiro("Selecionar (0=cancelar): ", 0, (int)r["data"].arr.size());
        if (sel==0) return "";
        ecraOk("Cliente: "+r["data"].arr[sel-1]["nome"].asString());
        return r["data"].arr[sel-1]["id"].asString();
    }

    /* Cliente unico */
    if (r["data"]["estado"].asString()=="suspenso") {
        ecraErr("Cliente suspenso: "+r["data"]["nome"].asString());
        pausar(); return "";
    }
    ecraOk("Cliente: "+r["data"]["nome"].asString()+" ("+r["data"]["nif"].asString()+")");
    return r["data"]["id"].asString();
}

/* ================================================================
 * NOVA VENDA
 * ================================================================ */
static void novaVenda() {
    g_dica = "0-Cancelar  Enter=Confirmar";
    iniciarEcra("","VENDA / FATURA","Nova venda");

    /* 1. Verificar caixa aberta */
    JsonValue cx = clientCmd(CMD_CX_ESTADO);
    if (!cx["ok"].asBool() || cx["data"]["estado"].asString()!="aberta") {
        ecraErr("Caixa nao esta aberta! Abra a caixa primeiro (opcao C).");
        pausar(); return;
    }

    /* 2. Selecionar cliente */
    std::string cli_id = promptCliente("PESQUISAR CLIENTE");
    if (cli_id.empty()) return;

    /* 3. Verificar pontos de fidelizacao */
    JsonObject fid_req; fid_req["cliente_id"]=JsonValue(cli_id);
    JsonValue fid = clientCmd(CMD_FID_SALDO, fid_req);
    int pontos = 0;
    if (fid["ok"].asBool()) pontos=(int)fid["data"]["pontos"].asInt();
    if (pontos>0) {
        std::ostringstream o; o<<"  Cliente tem "<<pontos<<" pontos ("<<std::fixed<<std::setprecision(2)<<(pontos*0.5)<<" EUR desconto)";
        ecraInfo(o.str());
    }

    /* 4. Adicionar itens */
    JsonArray itens;
    double total = 0.0;
    double desconto_fid = 0.0;

    while (true) {
        ecraVazia();
        ecraSep("ADICIONAR ARTIGO");
        ecraLinha("  1-EAN (codigo de barras)");
        ecraLinha("  2-Pesquisar por nome");
        ecraLinha("  0-Finalizar e faturar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='0') break;

        JsonValue produto;
        if (op=='1') {
            std::string ean = lerString("EAN: ");
            if (ean.empty()) continue;
            JsonObject d; d["ean"]=JsonValue(ean);
            JsonValue r = clientCmd(CMD_PRD_BUSCAR_EAN, d);
            if (!clientOk(r)) { pausar(); continue; }
            produto = r["data"];
        } else if (op=='2') {
            std::string nome = lerString("Nome (parte): ");
            if (nome.empty()) continue;
            JsonObject d; d["filtro"]=JsonValue(nome);
            JsonValue r = clientCmd(CMD_PRD_LISTAR, d);
            if (!r["ok"].asBool()||!r["data"].isArray()||r["data"].arr.empty()) {
                ecraErr("Sem resultados"); pausar(); continue;
            }
            for (size_t i=0;i<r["data"].arr.size()&&i<10;i++) {
                std::ostringstream o;
                long long stk=r["data"].arr[i]["stock_loja"].asInt();
                o<<"  "<<(i+1)<<". "<<fw(r["data"].arr[i]["nome"].asString(),30)
                 <<"  "<<std::fixed<<std::setprecision(2)<<r["data"].arr[i]["preco_venda"].asDouble()<<" EUR"
                 <<"  Stock: "<<stk;
                ecraLinha(o.str());
            }
            fecharEcra();
            int sel=lerInteiro("Selecionar: ",1,(int)std::min(r["data"].arr.size(),(size_t)10));
            produto = r["data"].arr[sel-1];
        } else continue;

        if (produto.isNull()) continue;
        if (!produto["ativo"].asBool()) { ecraErr("Produto inativo"); continue; }

        long long stk = produto["stock_loja"].asInt();
        std::ostringstream info;
        info << "  " << produto["nome"].asString()
             << "  " << std::fixed << std::setprecision(2) << produto["preco_venda"].asDouble() << " EUR";
        if (produto["tipo"].asString()=="produto") info << "  (Stock: " << stk << ")";
        ecraLinha(info.str());

        int qty = lerInteiro("Quantidade: ",1,9999);
        if (produto["tipo"].asString()=="produto" && stk<qty) {
            ecraErr("Stock insuficiente (disponivel: "+std::to_string(stk)+")"); continue;
        }

        double preco = produto["preco_venda"].asDouble();
        if (lerSimNao("Aplicar desconto?")) {
            double desc = lerDouble("Desconto (%): ");
            if (desc>0&&desc<100) { preco=preco*(1.0-desc/100.0); }
        }

        JsonObject item;
        item["produto_id"]     =JsonValue(produto["id"].asString());
        item["nome"]           =JsonValue(produto["nome"].asString());
        item["ean"]            =JsonValue(produto["ean"].asString());
        item["tipo"]           =JsonValue(produto["tipo"].asString());
        item["quantidade"]     =JsonValue((long long)qty);
        item["preco_unitario"] =JsonValue(preco);
        item["subtotal"]       =JsonValue(preco*qty);
        item["tem_garantia"]   =JsonValue(produto["tem_garantia"].asBool());
        item["duracao_garantia"]=JsonValue(produto["duracao_garantia"].asInt());

        itens.push_back(JsonValue(item));
        total += preco*qty;
        std::ostringstream o;
        o<<"  [+] "<<produto["nome"].asString()<<" x"<<qty
         <<"  = "<<std::fixed<<std::setprecision(2)<<(preco*qty)<<" EUR";
        ecraOk(o.str());
    }

    if (itens.empty()) { ecraErr("Sem itens. Venda cancelada."); pausar(); return; }

    /* 5. Usar pontos de fidelizacao */
    if (pontos>=20 && lerSimNao("Usar pontos de fidelizacao? ("+std::to_string(pontos)+" pts = "+
        std::to_string((int)(pontos/2))+" EUR)?")) {
        int pts_usar = std::min(pontos, (int)(total/0.5)); /* no maximo 50% do total */
        pts_usar = (pts_usar/10)*10; /* multiplo de 10 */
        if (pts_usar>0) {
            JsonObject fr; fr["cliente_id"]=JsonValue(cli_id); fr["pontos"]=JsonValue((long long)pts_usar);
            JsonValue fr2 = clientCmd(CMD_FID_RESGATAR, fr);
            if (fr2["ok"].asBool()) {
                desconto_fid = fr2["data"]["desconto_eur"].asDouble();
                total -= desconto_fid;
                std::ostringstream o; o<<"Desconto fidelizacao: -"<<std::fixed<<std::setprecision(2)<<desconto_fid<<" EUR";
                ecraOk(o.str());
            }
        }
    }

    /* 6. Metodo de pagamento */
    ecraVazia();
    ecraSep("PAGAMENTO");
    ecraLinha("  1-Dinheiro");
    ecraLinha("  2-Multibanco (MB)");
    ecraLinha("  3-Cartao (TPA)");
    ecraLinha("  4-MBWay");
    ecraLinha("  5-Credito");
    ecraLinha("  6-Cheque");
    fecharEcra();
    char pag = lerOpcaoMenu("Metodo: ");
    std::string pagamento;
    if(pag=='1') pagamento="Dinheiro";
    else if(pag=='2') pagamento="MB";
    else if(pag=='3') pagamento="Cartao";
    else if(pag=='4') pagamento="MBWay";
    else if(pag=='5') pagamento="Credito";
    else if(pag=='6') pagamento="Cheque";
    else { ecraErr("Pagamento invalido"); pausar(); return; }

    /* 7. Resumo */
    ecraVazia();
    ecraSep("RESUMO DA VENDA");
    for (size_t i=0;i<itens.size();i++) {
        std::ostringstream o;
        o<<"  "<<fw(itens[i]["nome"].asString(),30)
         <<"  x"<<itens[i]["quantidade"].asInt()
         <<"  "<<std::fixed<<std::setprecision(2)<<itens[i]["subtotal"].asDouble()<<" EUR";
        ecraLinha(o.str());
    }
    if (desconto_fid>0) {
        std::ostringstream o; o<<"  Desconto fidelizacao: -"<<std::fixed<<std::setprecision(2)<<desconto_fid<<" EUR";
        ecraLinha(o.str());
    }
    ecraSep();
    std::ostringstream tot; tot<<"  TOTAL: "<<std::fixed<<std::setprecision(2)<<total<<" EUR  ["<<pagamento<<"]";
    ecraLinha(tot.str());
    fecharEcra();

    if (!lerSimNao("Confirmar venda?")) { ecraErr("Venda cancelada."); pausar(); return; }

    /* 8. Enviar ao servidor */
    JsonObject d;
    d["cliente_id"] = JsonValue(cli_id);
    d["itens"]      = JsonValue(itens);
    d["total"]      = JsonValue(total);
    d["pagamento"]  = JsonValue(pagamento);
    d["desconto"]   = JsonValue(desconto_fid);

    JsonValue resp = clientCmd(CMD_VND_CRIAR, d);
    if (!clientOk(resp)) { pausar(); return; }

    std::string numero = resp["data"]["numero"].asString();
    ecraOk("Venda concluida: "+numero);
    ecraOk("Total: "+std::to_string(total)+" EUR");

    if (resp["data"]["garantias"].isArray() && !resp["data"]["garantias"].arr.empty()) {
        ecraVazia();
        ecraInfo("Garantias criadas automaticamente:");
        for (size_t i=0;i<resp["data"]["garantias"].arr.size();i++)
            ecraLinha("  - "+resp["data"]["garantias"].arr[i].asString());
    }
    pausar();
}

/* ================================================================
 * DEVOLUCAO
 * ================================================================ */
static void novaDevolucao() {
    iniciarEcra("","DEVOLUCAO","");
    std::string num_fat = lerString("Numero da fatura (ex: FAT-000001): ");
    if (num_fat.empty()) return;

    /* Obter detalhe da venda */
    JsonObject dr; dr["numero"]=JsonValue(num_fat);
    JsonValue venda = clientCmd(CMD_VND_DETALHE, dr);
    if (!clientOk(venda)) { pausar(); return; }

    ecraVazia();
    ecraLinha("  Venda: "+num_fat+"  Data: "+venda["data"]["data"].asString().substr(0,10));
    ecraVazia();
    ecraLinha("  Itens:");
    if (venda["data"]["itens"].isArray())
        for (size_t i=0;i<venda["data"]["itens"].arr.size();i++) {
            std::ostringstream o;
            o<<"  "<<(i+1)<<". "<<fw(venda["data"]["itens"].arr[i]["nome"].asString(),28)
             <<"  x"<<venda["data"]["itens"].arr[i]["quantidade"].asInt()
             <<"  "<<std::fixed<<std::setprecision(2)<<venda["data"]["itens"].arr[i]["subtotal"].asDouble()<<" EUR";
            ecraLinha(o.str());
        }

    std::string motivo = lerString("Motivo da devolucao: ");

    ecraLinha("  Metodo de reembolso: 1-Dinheiro  2-MB  3-Nota Credito");
    fecharEcra();
    char mr = lerOpcaoMenu("Metodo: ");
    std::string metodo=(mr=='1')?"Dinheiro":(mr=='2')?"MB":"Nota Credito";

    JsonObject d;
    d["numero_fatura"] = JsonValue(num_fat);
    d["itens"]         = venda["data"]["itens"];
    d["motivo"]        = JsonValue(motivo);
    d["metodo_reemb"]  = JsonValue(metodo);

    JsonValue r = clientCmd(CMD_DEV_CRIAR, d);
    if (!clientOk(r)) { pausar(); return; }
    ecraOk("Devolucao: "+r["data"]["numero"].asString()+
           "  Total: "+std::to_string(r["data"]["total"].asDouble())+" EUR");
    pausar();
}

/* ================================================================
 * CAIXA
 * ================================================================ */
static void menuCaixa() {
    while (true) {
        iniciarEcra("","CAIXA","9-Voltar");
        JsonValue cx = clientCmd(CMD_CX_ESTADO);
        if (cx["ok"].asBool()) {
            if (cx["data"]["estado"].asString()=="aberta") {
                ecraLinha("  Estado: ABERTA");
                std::ostringstream o;
                o<<"  Total vendas hoje: "<<std::fixed<<std::setprecision(2)<<cx["data"]["total_vendas"].asDouble()<<" EUR";
                ecraLinha(o.str());
                ecraVazia();
                ecraLinha("  2-Fechar caixa");
                ecraLinha("  3-Ver movimentos");
            } else {
                ecraLinha("  Estado: FECHADA");
                ecraVazia();
                ecraLinha("  1-Abrir caixa");
            }
        }
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;
        else if (op=='1') {
            double fundo = lerDouble("Fundo de caixa (EUR): ");
            JsonObject d; d["fundo"]=JsonValue(fundo);
            JsonValue r = clientCmd(CMD_CX_ABRIR, d);
            clientOk(r) ? ecraOk("Caixa aberta.") : void(0);
            pausar();
        } else if (op=='2') {
            double contagem = lerDouble("Contagem final (EUR): ");
            JsonObject d; d["contagem"]=JsonValue(contagem);
            JsonValue r = clientCmd(CMD_CX_FECHAR, d);
            if (r["ok"].asBool()) {
                ecraOk("Caixa fechada.");
                std::ostringstream o;
                o<<"  Fundo: "<<r["data"]["fundo"].asDouble()<<" EUR\n"
                 <<"  Vendas: "<<r["data"]["vendas"].asDouble()<<" EUR\n"
                 <<"  Esperado: "<<r["data"]["esperado"].asDouble()<<" EUR\n"
                 <<"  Contagem: "<<r["data"]["contagem"].asDouble()<<" EUR\n"
                 <<"  Diferenca: "<<r["data"]["diferenca"].asDouble()<<" EUR";
                ecraLinha(o.str());
            }
            pausar();
        } else if (op=='3') {
            JsonValue movs = clientCmd(CMD_CX_MOVIMENTOS);
            if (movs["ok"].asBool() && movs["data"].isArray()) {
                for (size_t i=0;i<movs["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<fw(movs["data"].arr[i]["hora"].asString().substr(11,8),10)
                     <<" "<<fw(movs["data"].arr[i]["tipo"].asString(),10)
                     <<" "<<fw(movs["data"].arr[i]["referencia"].asString(),14)
                     <<" "<<std::fixed<<std::setprecision(2)<<movs["data"].arr[i]["valor"].asDouble()<<" EUR";
                    ecraLinha(o.str());
                }
            }
            pausar();
        }
    }
}

/* ================================================================
 * REPARACOES
 * ================================================================ */
static void menuReparacoes() {
    while (true) {
        iniciarEcra("","REPARACOES","9-Voltar");
        ecraLinha("  1-Nova reparacao");
        ecraLinha("  2-Listar reparacoes");
        ecraLinha("  3-Atualizar estado");
        ecraLinha("  4-Concluir reparacao");
        ecraLinha("  5-Reparacoes em curso");
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        if (op=='1') {
            /* Nova reparacao */
            iniciarEcra("","NOVA REPARACAO","");
            std::string cli_id = promptCliente("CLIENTE DA REPARACAO");
            if (cli_id.empty()) continue;
            JsonObject d;
            d["cliente_id"]  = JsonValue(cli_id);
            d["equipamento"] = JsonValue(lerString("Equipamento (ex: iPhone 14): "));
            d["marca"]       = JsonValue(lerString("Marca/Modelo: "));
            d["serie"]       = JsonValue(lerString("N serie / IMEI (opcional): "));
            d["problema"]    = JsonValue(lerString("Descricao do problema: "));
            d["acessorios"]  = JsonValue(lerString("Acessorios entregues: "));
            d["senha"]       = JsonValue(lerString("Senha/PIN (opcional): "));
            d["orcamento"]   = JsonValue(lerDouble("Orcamento inicial (0=sem): "));
            d["tecnico"]     = JsonValue(lerString("Tecnico (Enter="+g_sessao.username+"): "));
            JsonValue r = clientCmd(CMD_REP_CRIAR, d);
            clientOk(r) ? ecraOk("Reparacao criada: "+r["data"]["numero"].asString()) : void(0);
            pausar();

        } else if (op=='2' || op=='5') {
            JsonObject d;
            if (op=='5') d["estado"]=JsonValue(std::string("recebido"));
            JsonValue r = clientCmd(CMD_REP_LISTAR, d);
            if (r["ok"].asBool() && r["data"].isArray()) {
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<fw(r["data"].arr[i]["numero"].asString(),12)
                     <<" "<<fw(r["data"].arr[i]["equipamento"].asString(),18)
                     <<" "<<fw(r["data"].arr[i]["estado"].asString(),12)
                     <<" "<<r["data"].arr[i]["tecnico"].asString();
                    ecraLinha(o.str());
                }
            }
            pausar();

        } else if (op=='3') {
            std::string num = lerString("Numero (ex: REP-000001): ");
            ecraLinha("  Estados: recebido | diagnostico | reparacao | concluido | entregue");
            std::string estado = lerString("Novo estado: ");
            std::string notas  = lerString("Notas (opcional): ");
            JsonObject d;
            d["numero"]=JsonValue(num); d["estado"]=JsonValue(estado); d["notas"]=JsonValue(notas);
            JsonValue r = clientCmd(CMD_REP_ESTADO, d);
            clientOk(r) ? ecraOk("Estado atualizado.") : void(0);
            pausar();

        } else if (op=='4') {
            std::string num = lerString("Numero da reparacao: ");
            JsonObject d;
            d["numero"]      = JsonValue(num);
            d["valor_final"] = JsonValue(lerDouble("Valor final (EUR): "));
            d["diagnostico"] = JsonValue(lerString("Trabalho realizado: "));
            d["pecas"]       = JsonValue(lerString("Pecas substituidas: "));
            JsonValue r = clientCmd(CMD_REP_CONCLUIR, d);
            if (clientOk(r))
                ecraOk("Reparacao concluida. Garantia ate: "+r["data"]["garantia_ate"].asString());
            pausar();
        }
    }
}

/* ================================================================
 * CLIENTES
 * ================================================================ */
static void menuClientes() {
    while (true) {
        iniciarEcra("","CLIENTES","9-Voltar");
        ecraLinha("  1-Criar cliente");
        ecraLinha("  2-Pesquisar cliente");
        ecraLinha("  3-Editar cliente");
        ecraLinha("  4-Historico do cliente");
        ecraLinha("  5-Pontos de fidelizacao");
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        if (op=='1') {
            iniciarEcra("","CRIAR CLIENTE","");
            JsonObject d;
            d["nome"]     = JsonValue(lerString("Nome: "));
            d["telefone"] = JsonValue(lerString("Telefone: "));
            d["email"]    = JsonValue(lerString("Email: "));
            d["nif"]      = JsonValue(lerString("NIF: "));
            JsonValue r = clientCmd(CMD_CLI_CRIAR, d);
            clientOk(r) ? ecraOk("Cliente criado: "+r["data"]["nome"].asString()) : void(0);
            pausar();

        } else if (op=='2') {
            std::string campo = lerString("Campo (nif/telefone/email/nome): ");
            std::string valor = lerString("Valor: ");
            JsonObject d; d["campo"]=JsonValue(campo); d["valor"]=JsonValue(valor);
            JsonValue r = clientCmd(CMD_CLI_BUSCAR, d);
            if (r["ok"].asBool()) {
                if (r["data"].isArray()) {
                    for (size_t i=0;i<r["data"].arr.size();i++) {
                        std::ostringstream o;
                        o<<"  "<<fw(r["data"].arr[i]["nome"].asString(),25)
                         <<" NIF:"<<fw(r["data"].arr[i]["nif"].asString(),12)
                         <<" Tel:"<<r["data"].arr[i]["telefone"].asString();
                        ecraLinha(o.str());
                    }
                } else {
                    std::ostringstream o;
                    o<<"  "<<r["data"]["nome"].asString()
                     <<" | NIF:"<<r["data"]["nif"].asString()
                     <<" | Tel:"<<r["data"]["telefone"].asString()
                     <<" | "<<r["data"]["estado"].asString();
                    ecraLinha(o.str());
                }
            } else mostrarErro(r);
            pausar();

        } else if (op=='3') {
            std::string id = lerString("ID ou NIF do cliente: ");
            JsonObject d; d["id"]=JsonValue(id);
            ecraLinha("  Campos: nome | telefone | email | nif");
            std::string campo = lerString("Campo a alterar: ");
            std::string valor = lerString("Novo valor: ");
            d[campo] = JsonValue(valor);
            JsonValue r = clientCmd(CMD_CLI_EDITAR, d);
            clientOk(r) ? ecraOk("Cliente atualizado.") : void(0);
            pausar();

        } else if (op=='4') {
            std::string cli_id = promptCliente("HISTORICO DO CLIENTE");
            if (cli_id.empty()) continue;
            JsonObject d; d["cliente_id"]=JsonValue(cli_id);
            JsonValue r = clientCmd(CMD_CLI_HISTORICO, d);
            if (r["ok"].asBool()) {
                ecraSep("VENDAS");
                if(r["data"]["vendas"].isArray())
                    for(size_t i=0;i<r["data"]["vendas"].arr.size();i++) {
                        std::ostringstream o;
                        o<<"  "<<r["data"]["vendas"].arr[i]["numero"].asString()
                         <<"  "<<r["data"]["vendas"].arr[i]["data"].asString().substr(0,10)
                         <<"  "<<std::fixed<<std::setprecision(2)<<r["data"]["vendas"].arr[i]["total"].asDouble()<<" EUR";
                        ecraLinha(o.str());
                    }
                ecraSep("REPARACOES");
                if(r["data"]["reparacoes"].isArray())
                    for(size_t i=0;i<r["data"]["reparacoes"].arr.size();i++) {
                        std::ostringstream o;
                        o<<"  "<<r["data"]["reparacoes"].arr[i]["numero"].asString()
                         <<"  "<<r["data"]["reparacoes"].arr[i]["equipamento"].asString()
                         <<"  ["<<r["data"]["reparacoes"].arr[i]["estado"].asString()<<"]";
                        ecraLinha(o.str());
                    }
                ecraSep("GARANTIAS");
                std::string hoje=dataApenasData();
                if(r["data"]["garantias"].isArray())
                    for(size_t i=0;i<r["data"]["garantias"].arr.size();i++) {
                        std::string fim=r["data"]["garantias"].arr[i]["data_fim"].asString();
                        std::ostringstream o;
                        o<<"  "<<r["data"]["garantias"].arr[i]["item"].asString()
                         <<"  ate "<<fim<<(fim>=hoje?" [VALIDA]":" [EXPIRADA]");
                        ecraLinha(o.str());
                    }
            }
            pausar();

        } else if (op=='5') {
            std::string cli_id = promptCliente("PONTOS FIDELIZACAO");
            if (cli_id.empty()) continue;
            JsonObject d; d["cliente_id"]=JsonValue(cli_id);
            JsonValue r = clientCmd(CMD_FID_SALDO, d);
            if (r["ok"].asBool()) {
                int pts=(int)r["data"]["pontos"].asInt();
                std::ostringstream o;
                o<<"  Pontos: "<<pts<<"  (= "<<std::fixed<<std::setprecision(2)<<(pts*0.5)<<" EUR de desconto)";
                ecraLinha(o.str());
            }
            pausar();
        }
    }
}

/* ================================================================
 * PRODUTOS
 * ================================================================ */
static void menuProdutos() {
    while (true) {
        iniciarEcra("","PRODUTOS","9-Voltar");
        ecraLinha("  1-Listar catalogo");
        ecraLinha("  2-Pesquisar por EAN");
        ecraLinha("  3-Pesquisar por nome");
        ecraLinha("  4-Alertas de stock");
        if (temPermissao("gerente")) {
            ecraLinha("  5-Criar produto/servico");
            ecraLinha("  6-Editar produto");
            ecraLinha("  7-Transferir stock entre lojas");
        }
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        if (op=='1') {
            JsonObject d;
            JsonValue r = clientCmd(CMD_PRD_LISTAR, d);
            if (r["ok"].asBool() && r["data"].isArray()) {
                std::cout << "\n" << fw("NOME",28) << fw("TIPO",10) << fw("PRECO",10) << fw("STOCK",8) << "EAN\n";
                linha();
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<fw(r["data"].arr[i]["nome"].asString(),28)
                     <<fw(r["data"].arr[i]["tipo"].asString(),10)
                     <<std::fixed<<std::setprecision(2)<<std::setw(8)<<r["data"].arr[i]["preco_venda"].asDouble()<<"  "
                     <<std::setw(6)<<r["data"].arr[i]["stock_loja"].asInt()<<"  "
                     <<r["data"].arr[i]["ean"].asString();
                    ecraLinha(o.str());
                }
            }
            pausar();

        } else if (op=='2') {
            std::string ean = lerString("EAN: ");
            JsonObject d; d["ean"]=JsonValue(ean);
            JsonValue r = clientCmd(CMD_PRD_BUSCAR_EAN, d);
            if (clientOk(r)) {
                std::ostringstream o;
                o<<"  Nome: "<<r["data"]["nome"].asString()<<"\n"
                 <<"  Preco: "<<std::fixed<<std::setprecision(2)<<r["data"]["preco_venda"].asDouble()<<" EUR\n"
                 <<"  Stock: "<<r["data"]["stock_loja"].asInt()<<"\n"
                 <<"  Garantia: "<<(r["data"]["tem_garantia"].asBool()?std::to_string((int)r["data"]["duracao_garantia"].asInt())+"d":"Nao");
                ecraLinha(o.str());
            }
            pausar();

        } else if (op=='3') {
            std::string nome = lerString("Nome (parte): ");
            JsonObject d; d["filtro"]=JsonValue(nome);
            JsonValue r = clientCmd(CMD_PRD_LISTAR, d);
            if (r["ok"].asBool() && r["data"].isArray())
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<fw(r["data"].arr[i]["nome"].asString(),28)
                     <<"  "<<std::fixed<<std::setprecision(2)<<r["data"].arr[i]["preco_venda"].asDouble()<<" EUR"
                     <<"  Stock: "<<r["data"].arr[i]["stock_loja"].asInt();
                    ecraLinha(o.str());
                }
            pausar();

        } else if (op=='4') {
            JsonValue r = clientCmd(CMD_PRD_STOCK_BAIXO);
            if (r["ok"].asBool() && r["data"].isArray()) {
                ecraLinha("  Produtos com stock baixo:");
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  [!] "<<fw(r["data"].arr[i]["nome"].asString(),28)
                     <<"  Stock: "<<r["data"].arr[i]["stock_loja"].asInt()
                     <<"  (min: "<<r["data"].arr[i]["stock_minimo"].asInt()<<")";
                    ecraLinha(o.str());
                }
            }
            pausar();

        } else if (op=='5' && temPermissao("gerente")) {
            iniciarEcra("","CRIAR PRODUTO","");
            JsonObject d;
            ecraLinha("  Tipo: 1=Produto  2=Servico");
            char t2=lerOpcaoMenu("Tipo: ");
            d["tipo"]             =JsonValue(t2=='1'?std::string("produto"):std::string("servico"));
            d["nome"]             =JsonValue(lerString("Nome: "));
            d["descricao"]        =JsonValue(lerString("Descricao: "));
            d["preco_venda"]      =JsonValue(lerDouble("Preco de venda (EUR): "));
            d["preco_custo"]      =JsonValue(lerDouble("Preco de custo (EUR): "));
            d["stock"]            =JsonValue((long long)(t2=='1'?lerInteiro("Stock inicial: ",0):0));
            d["stock_minimo"]     =JsonValue((long long)(t2=='1'?lerInteiro("Stock minimo alerta: ",0):0));
            d["ean"]              =JsonValue(t2=='1'?lerString("EAN (codigo de barras): "):std::string(""));
            d["tem_garantia"]     =JsonValue(lerSimNao("Tem garantia?"));
            d["duracao_garantia"] =JsonValue((long long)(d["tem_garantia"].asBool()?lerInteiro("Duracao garantia (dias): ",1):0));
            d["fornecedor_id"]    =JsonValue(std::string(""));
            JsonValue r = clientCmd(CMD_PRD_CRIAR, d);
            clientOk(r) ? ecraOk("Produto criado.") : void(0);
            pausar();

        } else if (op=='6' && temPermissao("gerente")) {
            std::string ean = lerString("EAN do produto: ");
            JsonObject d; d["ean"]=JsonValue(ean);
            JsonValue pr = clientCmd(CMD_PRD_BUSCAR_EAN, d);
            if (!clientOk(pr)) { pausar(); continue; }
            ecraLinha("  Produto: "+pr["data"]["nome"].asString());
            ecraLinha("  1-Preco venda  2-Preco custo  3-Stock minimo  4-Garantia");
            char f2=lerOpcaoMenu("Campo: ");
            JsonObject ed; ed["id"]=JsonValue(pr["data"]["id"].asString());
            if(f2=='1') ed["preco_venda"]=JsonValue(lerDouble("Novo preco venda: "));
            else if(f2=='2') ed["preco_custo"]=JsonValue(lerDouble("Novo preco custo: "));
            else if(f2=='3') ed["stock_minimo"]=JsonValue((long long)lerInteiro("Stock minimo: ",0));
            else if(f2=='4') { ed["tem_garantia"]=JsonValue(lerSimNao("Tem garantia?")); if(ed["tem_garantia"].asBool()) ed["duracao_garantia"]=JsonValue((long long)lerInteiro("Dias: ",1)); }
            JsonValue r = clientCmd(CMD_PRD_EDITAR, ed);
            clientOk(r) ? ecraOk("Produto atualizado.") : void(0);
            pausar();

        } else if (op=='7' && temPermissao("gerente")) {
            std::string ean = lerString("EAN do produto: ");
            JsonObject d2; d2["ean"]=JsonValue(ean);
            JsonValue pr = clientCmd(CMD_PRD_BUSCAR_EAN, d2);
            if (!clientOk(pr)) { pausar(); continue; }
            JsonObject td;
            td["produto_id"]   =JsonValue(pr["data"]["id"].asString());
            td["loja_origem"]  =JsonValue(lerString("Loja origem (ID): "));
            td["loja_destino"] =JsonValue(lerString("Loja destino (ID): "));
            td["quantidade"]   =JsonValue((long long)lerInteiro("Quantidade: ",1));
            JsonValue r = clientCmd(CMD_PRD_TRANS_STOCK, td);
            clientOk(r) ? ecraOk("Stock transferido.") : void(0);
            pausar();
        }
    }
}

/* ================================================================
 * ORCAMENTOS
 * ================================================================ */
static void menuOrcamentos() {
    while (true) {
        iniciarEcra("","ORCAMENTOS","9-Voltar");
        ecraLinha("  1-Novo orcamento");
        ecraLinha("  2-Listar orcamentos");
        ecraLinha("  3-Atualizar estado");
        ecraLinha("  4-Converter em fatura");
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        if (op=='1') {
            iniciarEcra("","NOVO ORCAMENTO","");
            std::string cli_id = promptCliente("CLIENTE DO ORCAMENTO");
            if (cli_id.empty()) continue;

            JsonArray itens;
            double total=0;
            while (true) {
                ecraLinha("  1-Add por EAN  2-Add por nome  0-Finalizar");
                fecharEcra();
                char ia=lerOpcaoMenu("Opcao: ");
                if(ia=='0') break;
                std::string ean_busca=(ia=='1')?lerString("EAN: "):lerString("Nome: ");
                JsonObject d2;
                if(ia=='1') d2["ean"]=JsonValue(ean_busca);
                else d2["filtro"]=JsonValue(ean_busca);
                JsonValue r=(ia=='1')?clientCmd(CMD_PRD_BUSCAR_EAN,d2):clientCmd(CMD_PRD_LISTAR,d2);
                if(!r["ok"].asBool()) continue;
                JsonValue prd = (ia=='1')?r["data"]:r["data"].arr[0];
                int qty=lerInteiro("Quantidade: ",1);
                double preco=prd["preco_venda"].asDouble();
                JsonObject item;
                item["produto_id"]=JsonValue(prd["id"].asString());
                item["nome"]=JsonValue(prd["nome"].asString());
                item["quantidade"]=JsonValue((long long)qty);
                item["preco_unitario"]=JsonValue(preco);
                item["subtotal"]=JsonValue(preco*qty);
                item["tem_garantia"]=JsonValue(prd["tem_garantia"].asBool());
                item["duracao_garantia"]=JsonValue(prd["duracao_garantia"].asInt());
                itens.push_back(JsonValue(item));
                total+=preco*qty;
                ecraOk("Adicionado: "+prd["nome"].asString());
            }
            JsonObject d;
            d["cliente_id"]=JsonValue(cli_id);
            d["itens"]=JsonValue(itens);
            d["total"]=JsonValue(total);
            d["observacoes"]=JsonValue(lerString("Observacoes: "));
            JsonValue r = clientCmd(CMD_ORC_CRIAR, d);
            clientOk(r) ? ecraOk("Orcamento criado: "+r["data"]["numero"].asString()) : void(0);
            pausar();

        } else if (op=='2') {
            JsonObject d;
            JsonValue r = clientCmd(CMD_ORC_LISTAR, d);
            if (r["ok"].asBool() && r["data"].isArray())
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<fw(r["data"].arr[i]["numero"].asString(),12)
                     <<" "<<fw(r["data"].arr[i]["estado"].asString(),12)
                     <<" "<<std::fixed<<std::setprecision(2)<<r["data"].arr[i]["total"].asDouble()<<" EUR";
                    ecraLinha(o.str());
                }
            pausar();

        } else if (op=='3') {
            std::string num=lerString("Numero (ORC-...): ");
            ecraLinha("  Estados: pendente | aprovado | rejeitado");
            std::string estado=lerString("Novo estado: ");
            JsonObject d; d["numero"]=JsonValue(num); d["estado"]=JsonValue(estado);
            JsonValue r = clientCmd(CMD_ORC_ESTADO, d);
            clientOk(r) ? ecraOk("Estado atualizado.") : void(0);
            pausar();

        } else if (op=='4') {
            std::string num=lerString("Numero do orcamento: ");
            ecraLinha("  Metodo pagamento: 1-Dinheiro  2-MB  3-Cartao  4-MBWay");
            fecharEcra();
            char pm=lerOpcaoMenu("Pagamento: ");
            std::string pag=(pm=='1')?"Dinheiro":(pm=='2')?"MB":(pm=='3')?"Cartao":"MBWay";
            JsonObject d; d["numero"]=JsonValue(num); d["pagamento"]=JsonValue(pag);
            JsonValue r = clientCmd(CMD_ORC_CONVERTER, d);
            if (clientOk(r)) ecraOk("Fatura criada: "+r["data"]["numero"].asString());
            pausar();
        }
    }
}

/* ================================================================
 * GARANTIAS
 * ================================================================ */
static void menuGarantias() {
    while (true) {
        iniciarEcra("","GARANTIAS","9-Voltar");
        ecraLinha("  1-Verificar garantia de cliente");
        ecraLinha("  2-Listar todas as garantias");
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        if (op=='1') {
            ecraLinha("  Pesquisar por: 1-NIF  2-Telefone");
            fecharEcra();
            char c2=lerOpcaoMenu("Opcao: ");
            std::string campo=(c2=='1')?"nif":"telefone";
            std::string valor=lerString(campo+": ");
            JsonObject d; d["campo"]=JsonValue(campo); d["valor"]=JsonValue(valor);
            JsonValue r = clientCmd(CMD_GAR_VERIFICAR, d);
            if (r["ok"].asBool() && r["data"].isArray()) {
                std::string hoje=dataApenasData();
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::string fim=r["data"].arr[i]["data_fim"].asString();
                    std::ostringstream o;
                    o<<"  "<<(fim>=hoje?"[VALIDA]  ":"[EXPIRADA]")
                     <<" "<<fw(r["data"].arr[i]["item"].asString(),28)
                     <<" ate "<<fim;
                    ecraLinha(o.str());
                }
            } else mostrarErro(r);
            pausar();

        } else if (op=='2') {
            JsonObject d;
            JsonValue r = clientCmd(CMD_GAR_LISTAR, d);
            if (r["ok"].asBool() && r["data"].isArray()) {
                std::string hoje=dataApenasData();
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::string fim=r["data"].arr[i]["data_fim"].asString();
                    std::ostringstream o;
                    o<<"  "<<(fim>=hoje?"[V]":"[X]")
                     <<" "<<fw(r["data"].arr[i]["item"].asString(),25)
                     <<" ate "<<fim;
                    ecraLinha(o.str());
                }
            }
            pausar();
        }
    }
}

/* ================================================================
 * RELATORIOS (gerente+)
 * ================================================================ */
static void menuRelatorios() {
    if (!temPermissao("gerente")) { erroPermissao(); return; }
    while (true) {
        iniciarEcra("","RELATORIOS","9-Voltar");
        ecraLinha("  1-Vendas por periodo");
        ecraLinha("  2-Stock critico");
        ecraLinha("  3-Top produtos");
        ecraLinha("  4-Performance por vendedor");
        ecraLinha("  5-Reparacoes");
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        std::string di,df;
        if (op=='1'||op=='3'||op=='4') {
            di=lerString("Data inicio (YYYY-MM-DD, Enter=tudo): ");
            df=lerString("Data fim (YYYY-MM-DD, Enter=hoje): ");
        }

        if (op=='1') {
            JsonObject d; d["data_ini"]=JsonValue(di); d["data_fim"]=JsonValue(df);
            JsonValue r = clientCmd(CMD_REL_VENDAS, d);
            if (r["ok"].asBool()) {
                std::ostringstream o;
                o<<"  Total: "<<std::fixed<<std::setprecision(2)<<r["data"]["total"].asDouble()
                 <<" EUR  | Vendas: "<<r["data"]["count"].asInt();
                ecraLinha(o.str());
                ecraSep("Por pagamento");
                if(r["data"]["por_pagamento"].isObject())
                    for(auto it=r["data"]["por_pagamento"].obj.begin();it!=r["data"]["por_pagamento"].obj.end();++it) {
                        std::ostringstream o2;
                        o2<<"  "<<fw(it->first,14)<<" "<<std::fixed<<std::setprecision(2)<<it->second.asDouble()<<" EUR";
                        ecraLinha(o2.str());
                    }
                ecraSep("Por loja");
                if(r["data"]["por_loja"].isObject())
                    for(auto it=r["data"]["por_loja"].obj.begin();it!=r["data"]["por_loja"].obj.end();++it) {
                        std::ostringstream o2;
                        o2<<"  "<<fw(it->first,14)<<" "<<std::fixed<<std::setprecision(2)<<it->second.asDouble()<<" EUR";
                        ecraLinha(o2.str());
                    }
            }
            pausar();
        } else if (op=='2') {
            JsonValue r = clientCmd(CMD_REL_STOCK);
            if (r["ok"].asBool()) {
                ecraSep("Stock zerado");
                if(r["data"]["zerado"].isArray())
                    for(size_t i=0;i<r["data"]["zerado"].arr.size();i++)
                        ecraLinha("  [0] "+r["data"]["zerado"].arr[i]["nome"].asString());
                ecraSep("Stock baixo");
                if(r["data"]["baixo"].isArray())
                    for(size_t i=0;i<r["data"]["baixo"].arr.size();i++) {
                        std::ostringstream o;
                        o<<"  [!] "<<r["data"]["baixo"].arr[i]["nome"].asString()
                         <<"  Stock: "<<r["data"]["baixo"].arr[i]["stock_loja"].asInt();
                        ecraLinha(o.str());
                    }
            }
            pausar();
        } else if (op=='3') {
            JsonObject d; d["data_ini"]=JsonValue(di); d["data_fim"]=JsonValue(df);
            JsonValue r = clientCmd(CMD_REL_TOP_PRODUTOS, d);
            if (r["ok"].asBool() && r["data"].isArray()) {
                ecraLinha("  # "+fw("PRODUTO",28)+" QTD    TOTAL");
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<std::setw(2)<<(i+1)<<" "
                     <<fw(r["data"].arr[i]["nome"].asString(),28)
                     <<std::setw(5)<<r["data"].arr[i]["quantidade"].asInt()
                     <<"  "<<std::fixed<<std::setprecision(2)<<r["data"].arr[i]["total"].asDouble()<<" EUR";
                    ecraLinha(o.str());
                }
            }
            pausar();
        } else if (op=='4') {
            JsonObject d; d["data_ini"]=JsonValue(di); d["data_fim"]=JsonValue(df);
            JsonValue r = clientCmd(CMD_REL_VENDEDOR, d);
            if (r["ok"].asBool() && r["data"].isArray()) {
                ecraLinha("  "+fw("VENDEDOR",10)+" VENDAS  TOTAL");
                for (size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<fw(r["data"].arr[i]["vendedor"].asString(),10)
                     <<std::setw(7)<<r["data"].arr[i]["vendas"].asInt()
                     <<"  "<<std::fixed<<std::setprecision(2)<<r["data"].arr[i]["total"].asDouble()<<" EUR";
                    ecraLinha(o.str());
                }
            }
            pausar();
        } else if (op=='5') {
            JsonObject d;
            JsonValue r = clientCmd(CMD_REL_REPARACOES, d);
            if (r["ok"].asBool()) {
                std::ostringstream o;
                o<<"  Total faturado: "<<std::fixed<<std::setprecision(2)<<r["data"]["total_faturado"].asDouble()<<" EUR";
                ecraLinha(o.str());
                if(r["data"]["reparacoes"].isArray())
                    for(size_t i=0;i<r["data"]["reparacoes"].arr.size();i++) {
                        std::ostringstream o2;
                        o2<<"  "<<fw(r["data"]["reparacoes"].arr[i]["numero"].asString(),12)
                          <<" "<<fw(r["data"]["reparacoes"].arr[i]["equipamento"].asString(),18)
                          <<" ["<<r["data"]["reparacoes"].arr[i]["estado"].asString()<<"]";
                        ecraLinha(o2.str());
                    }
            }
            pausar();
        }
    }
}

/* ================================================================
 * ADMINISTRACAO
 * ================================================================ */
static void menuAdmin() {
    while (true) {
        iniciarEcra("","ADMINISTRACAO","9-Voltar");
        ecraLinha("  1-Gestao de utilizadores");
        ecraLinha("  2-Gestao de lojas");
        ecraLinha("  3-Fornecedores");
        ecraLinha("  4-Logs do sistema");
        ecraLinha("  9-Voltar");
        fecharEcra();
        char op = lerOpcaoMenu("Opcao: ");
        if (op=='9') break;

        if (op=='1') {
            /* Utilizadores */
            while (true) {
                iniciarEcra("","UTILIZADORES","9-Voltar");
                ecraLinha("  1-Criar utilizador  2-Listar  3-Editar  9-Voltar");
                fecharEcra();
                char u2=lerOpcaoMenu("Opcao: ");
                if(u2=='9') break;
                if(u2=='1') {
                    iniciarEcra("","CRIAR UTILIZADOR","");
                    JsonObject d;
                    std::string user=lerString("Username (3 letras): ");
                    if(user.size()!=3){ecraErr("Username deve ter 3 letras");pausar();continue;}
                    d["username"]=JsonValue(user);
                    d["nome"]=JsonValue(lerString("Nome completo: "));
                    ecraLinha("  Roles: admin | gerente | vendedor | tecnico");
                    d["role"]=JsonValue(lerString("Role: "));
                    /* Listar lojas */
                    JsonValue lojas=clientCmd(CMD_LOJ_LISTAR);
                    if(lojas["ok"].asBool()&&lojas["data"].isArray()) {
                        ecraLinha("  Lojas disponiveis:");
                        for(size_t i=0;i<lojas["data"].arr.size();i++)
                            ecraLinha("  "+lojas["data"].arr[i]["id"].asString()+" - "+lojas["data"].arr[i]["nome"].asString());
                    }
                    d["loja_id"]=JsonValue(lerString("Loja ID (vazio=global): "));
                    std::string pw=lerString("Password: ");
                    d["password"]=JsonValue(hashPassword(pw));
                    JsonValue r=clientCmd(CMD_USR_CRIAR,d);
                    clientOk(r)?ecraOk("Utilizador criado."):void(0);
                    pausar();
                } else if(u2=='2') {
                    JsonValue r=clientCmd(CMD_USR_LISTAR);
                    if(r["ok"].asBool()&&r["data"].isArray())
                        for(size_t i=0;i<r["data"].arr.size();i++) {
                            std::ostringstream o;
                            o<<"  "<<fw(r["data"].arr[i]["username"].asString(),6)
                             <<" "<<fw(r["data"].arr[i]["nome"].asString(),22)
                             <<" "<<fw(r["data"].arr[i]["role"].asString(),10)
                             <<" "<<fw(r["data"].arr[i]["loja_id"].asString(),12)
                             <<" "<<r["data"].arr[i]["estado"].asString();
                            ecraLinha(o.str());
                        }
                    pausar();
                } else if(u2=='3') {
                    std::string user=lerString("Username: ");
                    ecraLinha("  1-Role  2-Estado  3-Loja  4-Password");
                    fecharEcra();
                    char f3=lerOpcaoMenu("Campo: ");
                    JsonObject d; d["username"]=JsonValue(user);
                    if(f3=='1') d["role"]=JsonValue(lerString("Novo role: "));
                    else if(f3=='2') d["estado"]=JsonValue(lerString("Novo estado (ativo/suspenso): "));
                    else if(f3=='3') d["loja_id"]=JsonValue(lerString("Nova loja ID: "));
                    else if(f3=='4') d["password"]=JsonValue(hashPassword(lerString("Nova password: ")));
                    JsonValue r=clientCmd(CMD_USR_EDITAR,d);
                    clientOk(r)?ecraOk("Utilizador atualizado."):void(0);
                    pausar();
                }
            }

        } else if (op=='2') {
            /* Lojas */
            while (true) {
                iniciarEcra("","LOJAS","9-Voltar");
                ecraLinha("  1-Criar loja  2-Listar  3-Editar  9-Voltar");
                fecharEcra();
                char l2=lerOpcaoMenu("Opcao: ");
                if(l2=='9') break;
                if(l2=='1') {
                    JsonObject d;
                    d["nome"]=JsonValue(lerString("Nome: "));
                    d["localizacao"]=JsonValue(lerString("Localizacao: "));
                    d["telefone"]=JsonValue(lerString("Telefone: "));
                    JsonValue r=clientCmd(CMD_LOJ_CRIAR,d);
                    if(clientOk(r)) ecraOk("Loja criada: ID="+r["data"]["id"].asString());
                    pausar();
                } else if(l2=='2') {
                    JsonValue r=clientCmd(CMD_LOJ_LISTAR);
                    if(r["ok"].asBool()&&r["data"].isArray())
                        for(size_t i=0;i<r["data"].arr.size();i++) {
                            std::ostringstream o;
                            o<<"  "<<fw(r["data"].arr[i]["id"].asString(),14)
                             <<" "<<fw(r["data"].arr[i]["nome"].asString(),22)
                             <<" "<<fw(r["data"].arr[i]["localizacao"].asString(),20)
                             <<" "<<r["data"].arr[i]["estado"].asString();
                            ecraLinha(o.str());
                        }
                    pausar();
                } else if(l2=='3') {
                    std::string lid=lerString("ID da loja: ");
                    ecraLinha("  1-Nome  2-Localizacao  3-Telefone  4-Estado");
                    fecharEcra();
                    char f3=lerOpcaoMenu("Campo: ");
                    JsonObject d; d["id"]=JsonValue(lid);
                    if(f3=='1') d["nome"]=JsonValue(lerString("Novo nome: "));
                    else if(f3=='2') d["localizacao"]=JsonValue(lerString("Nova localizacao: "));
                    else if(f3=='3') d["telefone"]=JsonValue(lerString("Novo telefone: "));
                    else if(f3=='4') d["estado"]=JsonValue(lerString("Estado (ativo/suspenso): "));
                    JsonValue r=clientCmd(CMD_LOJ_EDITAR,d);
                    clientOk(r)?ecraOk("Loja atualizada."):void(0);
                    pausar();
                }
            }

        } else if (op=='3') {
            /* Fornecedores */
            iniciarEcra("","FORNECEDORES","9-Voltar");
            ecraLinha("  1-Criar  2-Listar  9-Voltar");
            fecharEcra();
            char f2=lerOpcaoMenu("Opcao: ");
            if(f2=='1') {
                JsonObject d;
                d["nome"]=JsonValue(lerString("Nome: "));
                d["nif"]=JsonValue(lerString("NIF: "));
                d["email"]=JsonValue(lerString("Email: "));
                d["telefone"]=JsonValue(lerString("Telefone: "));
                d["morada"]=JsonValue(lerString("Morada: "));
                JsonValue r=clientCmd(CMD_FORN_CRIAR,d);
                clientOk(r)?ecraOk("Fornecedor criado."):void(0);
                pausar();
            } else if(f2=='2') {
                JsonValue r=clientCmd(CMD_FORN_LISTAR);
                if(r["ok"].asBool()&&r["data"].isArray())
                    for(size_t i=0;i<r["data"].arr.size();i++) {
                        std::ostringstream o;
                        o<<"  "<<fw(r["data"].arr[i]["nome"].asString(),25)
                         <<" NIF:"<<fw(r["data"].arr[i]["nif"].asString(),12)
                         <<" "<<r["data"].arr[i]["email"].asString();
                        ecraLinha(o.str());
                    }
                pausar();
            }

        } else if (op=='4') {
            JsonObject d; d["limite"]=JsonValue((long long)50);
            JsonValue r=clientCmd(CMD_LOG_LISTAR,d);
            if(r["ok"].asBool()&&r["data"].isArray())
                for(size_t i=0;i<r["data"].arr.size();i++) {
                    std::ostringstream o;
                    o<<"  "<<fw(r["data"].arr[i]["data"].asString().substr(11,8),10)
                     <<" "<<fw(r["data"].arr[i]["utilizador"].asString(),6)
                     <<" "<<fw(r["data"].arr[i]["loja_id"].asString(),8)
                     <<" "<<r["data"].arr[i]["acao"].asString();
                    if(!r["data"].arr[i]["detalhe"].asString().empty())
                        o<<" - "<<r["data"].arr[i]["detalhe"].asString().substr(0,30);
                    ecraLinha(o.str());
                }
            pausar();
        }
    }
}

/* ================================================================
 * DASHBOARD
 * ================================================================ */
static void dashboard() {
    iniciarEcra("","DASHBOARD","9-Voltar");
    JsonValue r = clientCmd(CMD_DASHBOARD);
    if (!r["ok"].asBool()) { mostrarErro(r); pausar(); return; }

    ecraVazia();
    std::cout<<" +--------------------------------+  +--------------------------------+\n";
    std::cout<<" |  Clientes registados           |  |  Produtos no catalogo          |\n";
    std::ostringstream s1,s2;
    s1<<std::setw(30)<<r["data"]["clientes"].asInt();
    s2<<std::setw(30)<<r["data"]["produtos"].asInt();
    std::cout<<" |"<<s1.str()<<"  |  |"<<s2.str()<<"  |\n";
    std::cout<<" +--------------------------------+  +--------------------------------+\n\n";
    std::cout<<" +--------------------------------+  +--------------------------------+\n";
    std::cout<<" |  Vendas hoje (EUR)             |  |  Vendas totais                 |\n";
    std::ostringstream s3,s4;
    s3<<std::fixed<<std::setprecision(2)<<std::setw(30)<<r["data"]["total_dia"].asDouble();
    s4<<std::setw(30)<<r["data"]["vendas"].asInt();
    std::cout<<" |"<<s3.str()<<"  |  |"<<s4.str()<<"  |\n";
    std::cout<<" +--------------------------------+  +--------------------------------+\n\n";
    std::cout<<" +--------------------------------+  +--------------------------------+\n";
    std::cout<<" |  Reparacoes em curso           |  |  Orcamentos pendentes          |\n";
    std::string rep_str=std::to_string((int)r["data"]["reps_curso"].asInt())+" / "+std::to_string((int)r["data"]["reps_total"].asInt());
    std::ostringstream s5,s6;
    s5<<std::setw(30)<<rep_str;
    s6<<std::setw(30)<<r["data"]["orcs_pendentes"].asInt();
    std::cout<<" |"<<s5.str()<<"  |  |"<<s6.str()<<"  |\n";
    std::cout<<" +--------------------------------+  +--------------------------------+\n";

    if (r["data"]["stock_baixo"].asInt()>0) {
        ecraVazia();
        ecraErr("ATENCAO: "+std::to_string((int)r["data"]["stock_baixo"].asInt())+" produto(s) com stock baixo!");
    }
    pausar();
}

/* ================================================================
 * MENU PRINCIPAL
 * ================================================================ */
static void menuPrincipal() {
    while (true) {
        g_dica = "SUB-MENU --------------------------------->";
        iniciarEcra("","MENU PRINCIPAL","");

        /* Cabecalho de tabela - estilo SSN */
        std::cout<<"\n";
        std::cout<<fw("ARTIGO",12)<<fw("QUANTID",10)<<fw("DESCRICAO",30)<<fw("T",3)<<fw("PRECO",10)<<"LJ\n";
        std::cout<<fw("------",12)<<fw("-------",10)<<fw("---------",30)<<fw("-",3)<<fw("-----",10)<<"--\n";
        std::cout<<"\n";

        std::vector<MenuItem> esq, dir;
        esq.push_back({"0","Venda / Nova Fatura"});
        esq.push_back({"1","Clientes"});
        esq.push_back({"2","Produtos / Catalogo"});
        esq.push_back({"3","Reparacoes"});
        esq.push_back({"4","Orcamentos"});
        esq.push_back({"5","Garantias"});
        esq.push_back({"6","Devolucao"});
        esq.push_back({"7","Dashboard"});
        esq.push_back({"D","Abrir / Fechar Caixa"});

        dir.push_back({"A","Pesquisar Cliente"});
        dir.push_back({"B","Nova Reparacao (rapida)"});
        dir.push_back({"C","Verificar Garantia"});
        dir.push_back({"G","Alertas de Stock"});
        if(temPermissao("gerente")) dir.push_back({"R","Relatorios"});
        if(temPermissao("admin"))   dir.push_back({"S","Administracao"});

        std::cout<<"\n";
        menuDuasColunas(esq, dir, 40);
        std::cout<<"\n";
        menuItem("9","Sair"); std::cout<<"   "; menuItem("F","Sair"); std::cout<<"\n";
        std::cout<<"\n";
        hline();
        std::cout<<ANSI_DIM<<"                                        SUB-TOTAL ---------------->"<<ANSI_RESET<<"\n";

        char op = lerOpcaoMenu("Opcao: ");
        switch(op) {
            case '0': novaVenda();         break;
            case '1': menuClientes();      break;
            case '2': menuProdutos();      break;
            case '3': menuReparacoes();    break;
            case '4': menuOrcamentos();    break;
            case '5': menuGarantias();     break;
            case '6': novaDevolucao();     break;
            case '7': dashboard();         break;
            case 'D': menuCaixa();         break;
            case 'A': {
                std::string ci=promptCliente("PESQUISAR CLIENTE");
                (void)ci; pausar();
                break; }
            case 'B': {
                iniciarEcra("","NOVA REPARACAO","");
                std::string ci=promptCliente("CLIENTE");
                if(!ci.empty()) {
                    JsonObject d;
                    d["cliente_id"]=JsonValue(ci);
                    d["equipamento"]=JsonValue(lerString("Equipamento: "));
                    d["marca"]=JsonValue(lerString("Marca: "));
                    d["serie"]=JsonValue(std::string(""));
                    d["problema"]=JsonValue(lerString("Problema: "));
                    d["acessorios"]=JsonValue(std::string(""));
                    d["senha"]=JsonValue(std::string(""));
                    d["orcamento"]=JsonValue(0.0);
                    d["tecnico"]=JsonValue(std::string(""));
                    JsonValue r=clientCmd(CMD_REP_CRIAR,d);
                    clientOk(r)?ecraOk("Reparacao: "+r["data"]["numero"].asString()):void(0);
                    pausar();
                }
                break; }
            case 'C': {
                iniciarEcra("","VERIFICAR GARANTIA","");
                ecraLinha("  1-NIF  2-Telefone");
                fecharEcra();
                char cg=lerOpcaoMenu("Opcao: ");
                std::string camp=(cg=='1')?"nif":"telefone";
                std::string val=lerString(camp+": ");
                JsonObject dg; dg["campo"]=JsonValue(camp); dg["valor"]=JsonValue(val);
                JsonValue rg=clientCmd(CMD_GAR_VERIFICAR,dg);
                if(rg["ok"].asBool()&&rg["data"].isArray()) {
                    std::string hoje=dataApenasData();
                    for(size_t i=0;i<rg["data"].arr.size();i++) {
                        std::string fim=rg["data"].arr[i]["data_fim"].asString();
                        ecraLinha("  "+(fim>=hoje?std::string("[VALIDA]   "):std::string("[EXPIRADA] "))+
                                  rg["data"].arr[i]["item"].asString()+" ate "+fim);
                    }
                } else ecraErr(rg["erro"].asString());
                pausar(); break; }
            case 'G': {
                JsonValue r=clientCmd(CMD_PRD_STOCK_BAIXO);
                if(r["ok"].asBool()&&r["data"].isArray())
                    for(size_t i=0;i<r["data"].arr.size();i++)
                        ecraLinha("  [!] "+fw(r["data"].arr[i]["nome"].asString(),28)+
                                  " Stock: "+std::to_string((int)r["data"].arr[i]["stock_loja"].asInt()));
                pausar(); break; }
            case 'R': menuRelatorios(); break;
            case 'S':
                if(temPermissao("admin")) menuAdmin();
                else erroPermissao();
                break;
            case '9':
            case 'F':
                iniciarEcra("","SAIR","");
                if(lerSimNao("Terminar sessao?")) return;
                break;
            default: break;
        }
    }
}

/* ================================================================
 * ECRA DE CONFIGURACAO (primeiro uso)
 * ================================================================ */
static bool configurarServidor() {
    cls();
    std::cout<<ANSI_BOLD<<ANSI_WHITE;
    std::cout<<"  TECHFIX - CONFIGURACAO INICIAL\n";
    std::cout<<"  ================================\n\n";
    std::cout<<ANSI_RESET;
    std::cout<<"  Ficheiro '"<<CLIENT_CONFIG_FILE<<"' nao encontrado.\n\n";

    ClientConfig cfg;
    cfg.host    = lerString("  Endereco do servidor (IP ou hostname): ");
    std::string porta = lerString("  Porta (Enter=2022): ");
    cfg.port    = porta.empty() ? PROTO_PORT : std::stoi(porta);
    cfg.loja_id = lerString("  ID desta loja (ex: loj_..., ou deixar em branco): ");

    guardarConfig(cfg);
    std::cout<<"\n  Configuracao guardada em '"<<CLIENT_CONFIG_FILE<<"'.\n\n";
    return true;
}

/* ================================================================
 * ECRA DE LOGIN
 * ================================================================ */
static bool ecrLogin() {
    iniciarEcra("LOGIN","","Introduza as credenciais");
    std::cout<<"\n";
    std::cout<<"  +-----------------------------------------------+\n";
    std::cout<<"  |       TECHFIX - SISTEMA DE GESTAO             |\n";
    std::cout<<"  |       Loja: "<<fw(g_client.cfg.loja_id.empty()?"N/D":g_client.cfg.loja_id,34)<<"|\n";
    std::cout<<"  |       Servidor: "<<fw(g_client.cfg.host+":"+std::to_string(g_client.cfg.port),30)<<"|\n";
    std::cout<<"  +-----------------------------------------------+\n\n";
    fecharEcra();

    std::string user = lerString("Username (3 letras): ");
    if (user.size()!=3) { ecraErr("Username invalido"); return false; }

    /* Password sem eco */
    std::cout<<"  Password: ";
    std::cout<<"\033[8m";
    std::string pw;
    std::getline(std::cin, pw);
    std::cout<<"\033[28m\n";
    while(!pw.empty()&&pw.back()=='\r') pw.pop_back();

    return clientLogin(user, pw);
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "");
    uiInitConsole();
    netInit();

    /* Argumento: --servidor host:port */
    for(int i=1;i<argc;++i) {
        if(strcmp(argv[i],"--servidor")==0 && i+1<argc) {
            std::string s(argv[++i]);
            size_t colon=s.rfind(':');
            if(colon!=std::string::npos){
                g_client.cfg.host=s.substr(0,colon);
                g_client.cfg.port=std::stoi(s.substr(colon+1));
            } else {
                g_client.cfg.host=s;
                g_client.cfg.port=PROTO_PORT;
            }
        }
        if(strcmp(argv[i],"--loja")==0 && i+1<argc) g_client.cfg.loja_id=argv[++i];
    }

    /* Carregar config se existir e nao foi passado por argumento */
    if(g_client.cfg.host=="127.0.0.1") {
        std::ifstream tf(CLIENT_CONFIG_FILE);
        if(!tf.is_open()) configurarServidor();
        g_client.cfg = carregarConfig();
    }

    /* Ligar ao servidor */
    cls();
    std::cout<<"\n  A ligar ao servidor "<<g_client.cfg.host<<":"<<g_client.cfg.port<<"...\n";
    std::cout.flush();

    if (!clientLigar(g_client.cfg.host, g_client.cfg.port)) {
        std::cout<<"\n  "<<ANSI_RED<<"[!!] Nao foi possivel ligar ao servidor!"<<ANSI_RESET<<"\n\n";
        std::cout<<"  Verifique:\n";
        std::cout<<"  1. O servidor gestao_server esta a correr?\n";
        std::cout<<"  2. O IP '"<<g_client.cfg.host<<"' esta correto?\n";
        std::cout<<"  3. A porta "<<g_client.cfg.port<<" esta acessivel (firewall)?\n\n";
        std::cout<<"  Prima Enter para sair...\n";
        std::cin.get();
        return 1;
    }
    std::cout<<"  Ligado ao servidor.\n";
    sleep(1);

    /* Loop de login */
    for(int i=0;i<3;++i) {
        if(ecrLogin()) {
            g_loja_nome = g_sessao.loja_nome.empty() ? g_client.cfg.loja_id : g_sessao.loja_nome;
            menuPrincipal();
            /* Logout */
            clientCmd(CMD_LOGOUT);
            break;
        }
        if(i<2) { pausar("Prima Enter para tentar novamente..."); }
        else    { ecraErr("Maximo de tentativas."); pausar(); }
    }

    NET_CLOSE(g_client.sock);
    netCleanup();
    cls();
    std::cout<<"\n  Sistema encerrado. Ate breve!\n\n";
    return 0;
}
