#pragma once
/*
 * server_handlers.h - Handlers de logica de negocio do servidor
 * Cada handler recebe o request JSON e retorna response JSON
 * Tem acesso direto aos ficheiros de dados (sem rede)
 */

#include "json_utils.h"
#include "sha256.h"
#include "net_utils.h"
#include "protocolo.h"
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>

#ifdef _WIN32
  #include <direct.h>
  #define SMKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define SMKDIR(p) mkdir(p,0755)
#endif

/* ── Paths de dados ─────────────────────────────────────────────── */
#define SRV_DATA   "data/"
#define SRV_DOCS   "docs/"
#define F_UTIL     SRV_DATA "utilizadores.json"
#define F_CLI      SRV_DATA "clientes.json"
#define F_PRD      SRV_DATA "produtos.json"
#define F_VND      SRV_DATA "vendas.json"
#define F_ORC      SRV_DATA "orcamentos.json"
#define F_REP      SRV_DATA "reparacoes.json"
#define F_GAR      SRV_DATA "garantias.json"
#define F_LOJ      SRV_DATA "lojas.json"
#define F_LOG      SRV_DATA "logs.json"
#define F_DEV      SRV_DATA "devolucoes.json"
#define F_CX       SRV_DATA "caixa.json"
#define F_FORN     SRV_DATA "fornecedores.json"
#define F_FID      SRV_DATA "fidelizacao.json"

/* ── Sessao autenticada por socket ──────────────────────────────── */
struct SrvSessao {
    std::string username;
    std::string nome;
    std::string role;
    std::string loja_id;
    bool autenticado{false};
};

/* ── Mutex RECURSIVO para ficheiros (evitar deadlock em chamadas aninhadas) ── */
#include <pthread.h>
static pthread_mutex_t* srvGetMtx() {
    static pthread_mutex_t mtx;
    static bool init=false;
    if(!init){
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mtx, &attr);
        pthread_mutexattr_destroy(&attr);
        init=true;
    }
    return &mtx;
}
struct FileLock {
    FileLock()  { pthread_mutex_lock(srvGetMtx());   }
    ~FileLock() { pthread_mutex_unlock(srvGetMtx()); }
};

/* ── Utilitarios de data ────────────────────────────────────────── */
static std::string srvDataAtual() {
    std::time_t t=std::time(0); char b[32];
    std::strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",std::localtime(&t));
    return b;
}
static std::string srvDataApenas() {
    std::time_t t=std::time(0); char b[16];
    std::strftime(b,sizeof(b),"%Y-%m-%d",std::localtime(&t));
    return b;
}
static std::string srvAdicionarDias(const std::string& ds, int dias) {
    struct tm tm2; memset(&tm2,0,sizeof(tm2));
    int y=0,mo=0,d=0; sscanf(ds.c_str(),"%d-%d-%d",&y,&mo,&d);
    tm2.tm_year=y-1900; tm2.tm_mon=mo-1; tm2.tm_mday=d;
    std::time_t t=std::mktime(&tm2); t+=(std::time_t)dias*86400;
    char buf[16]; std::strftime(buf,sizeof(buf),"%Y-%m-%d",std::localtime(&t));
    return buf;
}
static std::string srvGenId(const std::string& pfx) {
    static long long c=0;
    std::ostringstream o; o<<pfx<<std::time(0)<<"_"<<(++c);
    return o.str();
}
static int srvProxSeq(const JsonValue& arr, const std::string& campo_numero, const std::string& pfx) {
    int mx=0;
    if(arr.isArray())
        for(size_t i=0;i<arr.arr.size();i++){
            std::string n=arr.arr[i][campo_numero].asString();
            if(n.size()>pfx.size()) try{int v=std::stoi(n.substr(pfx.size()));if(v>mx)mx=v;}catch(...){}
        }
    return mx+1;
}

/* ── Verificar permissao ────────────────────────────────────────── */
static int nivelRole(const std::string& r){
    if(r=="admin")    return 4;
    if(r=="gerente")  return 3;
    if(r=="vendedor") return 2;
    if(r=="tecnico")  return 1;
    return 0;
}
static bool temPerm(const SrvSessao& s, const std::string& rm){
    return nivelRole(s.role)>=nivelRole(rm);
}
/* Verificar se o utilizador pertence a esta loja (ou e admin/gerente global) */
static bool temPermLoja(const SrvSessao& s, const std::string& loja_id){
    if(s.role=="admin") return true;
    if(s.role=="gerente" && s.loja_id.empty()) return true; /* gerente global */
    return s.loja_id==loja_id;
}

/* ── Log no servidor ────────────────────────────────────────────── */
static void srvLog(const std::string& user, const std::string& loja,
                   const std::string& acao, const std::string& detalhe="") {
    FileLock lk;
    JsonValue logs=jsonParseFile(F_LOG);
    if(!logs.isArray()) logs=JsonValue(JsonArray{});
    JsonObject e;
    e["data"]     =JsonValue(srvDataAtual());
    e["utilizador"]=JsonValue(user);
    e["loja_id"]  =JsonValue(loja);
    e["acao"]     =JsonValue(acao);
    e["detalhe"]  =JsonValue(detalhe);
    logs.arr.push_back(JsonValue(e));
    if(logs.arr.size()>10000) logs.arr.erase(logs.arr.begin(),logs.arr.begin()+(logs.arr.size()-10000));
    jsonSaveFile(F_LOG,logs);
}

/* ── Criar garantia automatica ──────────────────────────────────── */
static std::string srvGarantiaCriar(const std::string& cli_id, const std::string& item,
                                     int dias, const std::string& ref, const std::string& loja_id) {
    std::string d0=srvDataApenas(), df=srvAdicionarDias(d0,dias);
    JsonObject g;
    g["id"]          =JsonValue(srvGenId("gar_"));
    g["cliente_id"]  =JsonValue(cli_id);
    g["loja_id"]     =JsonValue(loja_id);
    g["item"]        =JsonValue(item);
    g["referencia"]  =JsonValue(ref);
    g["data_inicio"] =JsonValue(d0);
    g["data_fim"]    =JsonValue(df);
    g["duracao_dias"]=JsonValue((long long)dias);
    g["criado_em"]   =JsonValue(srvDataAtual());
    JsonValue gars=jsonParseFile(F_GAR);
    if(!gars.isArray()) gars=JsonValue(JsonArray{});
    gars.arr.push_back(JsonValue(g));
    jsonSaveFile(F_GAR,gars);
    return df;
}

/* ── Pontos de fidelizacao ──────────────────────────────────────── */
static void srvFidAdicionar(const std::string& cli_id, double total) {
    int pts=(int)(total/10); /* 1 ponto por cada 10 EUR */
    if(pts<=0) return;
    FileLock lk;
    JsonValue fid=jsonParseFile(F_FID);
    if(!fid.isArray()) fid=JsonValue(JsonArray{});
    for(size_t i=0;i<fid.arr.size();i++){
        if(fid.arr[i]["cliente_id"].asString()==cli_id){
            int atual=(int)fid.arr[i]["pontos"].asInt();
            fid.arr[i]["pontos"]=JsonValue((long long)(atual+pts));
            jsonSaveFile(F_FID,fid);
            return;
        }
    }
    JsonObject r; r["cliente_id"]=JsonValue(cli_id); r["pontos"]=JsonValue((long long)pts);
    fid.arr.push_back(JsonValue(r));
    jsonSaveFile(F_FID,fid);
}

/* ======================================================================
 * HANDLER: LOGIN
 * ====================================================================== */
inline JsonValue hLogin(const JsonValue& req, SrvSessao& sessao) {
    std::string user = req["data"]["username"].asString();
    std::string pw   = req["data"]["password"].asString(); /* ja em SHA-256 */
    std::string loja = req["data"]["loja_id"].asString();  /* loja que o cliente informa */

    JsonValue utils=jsonParseFile(F_UTIL);
    if(!utils.isArray()) return mkErr("Sem utilizadores configurados");

    for(size_t i=0;i<utils.arr.size();i++){
        JsonValue& u=utils.arr[i];
        if(u["username"].asString()==user && u["password"].asString()==pw){
            if(u["estado"].asString()!="ativo") return mkErr("Conta suspensa");

            std::string u_loja=u["loja_id"].asString();
            std::string role   =u["role"].asString();

            /* Verificar se o utilizador tem permissao para esta loja */
            if(role!="admin"){
                /* gerente global (loja_id vazio) pode ligar-se a qualquer loja */
                if(role=="gerente" && u_loja.empty()){
                    /* ok */
                } else if(!u_loja.empty() && u_loja!=loja){
                    return mkErr("Sem permissao para a loja "+loja+
                                 ". A sua loja e: "+u_loja);
                }
            }

            sessao.username   = user;
            sessao.nome       = u["nome"].asString();
            sessao.role       = role;
            sessao.loja_id    = loja.empty() ? u_loja : loja;
            sessao.autenticado= true;

            srvLog(user, sessao.loja_id, "LOGIN");

            JsonObject d;
            d["username"]=JsonValue(user);
            d["nome"]    =JsonValue(sessao.nome);
            d["role"]    =JsonValue(role);
            d["loja_id"] =JsonValue(sessao.loja_id);

            /* Carregar nome da loja */
            JsonValue lojas=jsonParseFile(F_LOJ);
            if(lojas.isArray())
                for(size_t j=0;j<lojas.arr.size();j++)
                    if(lojas.arr[j]["id"].asString()==sessao.loja_id)
                        d["loja_nome"]=JsonValue(lojas.arr[j]["nome"].asString());

            return mkOk(JsonValue(d));
        }
    }
    return mkErr("Credenciais invalidas");
}

/* ======================================================================
 * HANDLER: DASHBOARD
 * ====================================================================== */
inline JsonValue hDashboard(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string hoje=srvDataApenas();

    JsonValue clientes=jsonParseFile(F_CLI);
    JsonValue produtos=jsonParseFile(F_PRD);
    JsonValue vendas  =jsonParseFile(F_VND);
    JsonValue reps    =jsonParseFile(F_REP);
    JsonValue orcs    =jsonParseFile(F_ORC);

    int n_cli=0,n_prd=0,n_vnd=0,n_rep=0,n_rep_curso=0,n_orc_pend=0,n_stock=0;
    double total_dia=0.0;

    if(clientes.isArray()) n_cli=(int)clientes.arr.size();

    if(produtos.isArray())
        for(size_t i=0;i<produtos.arr.size();i++){
            JsonValue& p=produtos.arr[i];
            if(!p["ativo"].asBool()) continue;
            n_prd++;
            if(p["tipo"].asString()=="produto" && p["stock"].asInt()<=p["stock_minimo"].asInt()) n_stock++;
        }

    if(vendas.isArray())
        for(size_t i=0;i<vendas.arr.size();i++){
            JsonValue& v=vendas.arr[i];
            if(!temPermLoja(s,v["loja_id"].asString())) continue;
            n_vnd++;
            if(v["data"].asString().substr(0,10)==hoje) total_dia+=v["total"].asDouble();
        }

    if(reps.isArray())
        for(size_t i=0;i<reps.arr.size();i++){
            JsonValue& r=reps.arr[i];
            if(!temPermLoja(s,r["loja_id"].asString())) continue;
            n_rep++;
            std::string e=r["estado"].asString();
            if(e!="entregue"&&e!="concluido") n_rep_curso++;
        }

    if(orcs.isArray())
        for(size_t i=0;i<orcs.arr.size();i++){
            JsonValue& o=orcs.arr[i];
            if(!temPermLoja(s,o["loja_id"].asString())) continue;
            if(o["estado"].asString()=="pendente") n_orc_pend++;
        }

    JsonObject d;
    d["clientes"]        =JsonValue((long long)n_cli);
    d["produtos"]        =JsonValue((long long)n_prd);
    d["vendas"]          =JsonValue((long long)n_vnd);
    d["total_dia"]       =JsonValue(total_dia);
    d["reps_total"]      =JsonValue((long long)n_rep);
    d["reps_curso"]      =JsonValue((long long)n_rep_curso);
    d["orcs_pendentes"]  =JsonValue((long long)n_orc_pend);
    d["stock_baixo"]     =JsonValue((long long)n_stock);
    d["loja_id"]         =JsonValue(s.loja_id);
    return mkOk(JsonValue(d));
}

/* ======================================================================
 * HANDLER: CLIENTES
 * ====================================================================== */
inline JsonValue hCliCriar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    const JsonValue& d=req["data"];
    std::string nif=d["nif"].asString();
    std::string tel=d["telefone"].asString();

    FileLock lk;
    JsonValue clientes=jsonParseFile(F_CLI);
    if(!clientes.isArray()) clientes=JsonValue(JsonArray{});

    /* Verificar duplicados */
    for(size_t i=0;i<clientes.arr.size();i++){
        if(!nif.empty() && clientes.arr[i]["nif"].asString()==nif)
            return mkErr("NIF ja registado: "+clientes.arr[i]["nome"].asString());
        if(!tel.empty() && clientes.arr[i]["telefone"].asString()==tel)
            return mkErr("Telefone ja registado: "+clientes.arr[i]["nome"].asString());
    }

    JsonObject c;
    c["id"]           =JsonValue(srvGenId("cli_"));
    c["nome"]         =JsonValue(d["nome"].asString());
    c["telefone"]     =JsonValue(tel);
    c["email"]        =JsonValue(d["email"].asString());
    c["nif"]          =JsonValue(nif);
    c["estado"]       =JsonValue(std::string("ativo"));
    c["data_registo"] =JsonValue(srvDataAtual());
    c["pontos_fid"]   =JsonValue((long long)0);

    clientes.arr.push_back(JsonValue(c));
    jsonSaveFile(F_CLI,clientes);
    srvLog(s.username,s.loja_id,"CLI_CRIAR","nome="+d["nome"].asString());
    return mkOk(JsonValue(c));
}

inline JsonValue hCliBuscar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string campo=req["data"]["campo"].asString();
    std::string valor=req["data"]["valor"].asString();

    JsonValue clientes=jsonParseFile(F_CLI);
    if(!clientes.isArray()) return mkErr("Sem clientes");

    /* Busca exata por campo, ou parcial por nome */
    JsonArray result;
    std::string vLow=valor;
    std::transform(vLow.begin(),vLow.end(),vLow.begin(),::tolower);

    for(size_t i=0;i<clientes.arr.size();i++){
        JsonValue& c=clientes.arr[i];
        bool match=false;
        if(campo=="nome"){
            std::string n=c["nome"].asString();
            std::transform(n.begin(),n.end(),n.begin(),::tolower);
            match=(n.find(vLow)!=std::string::npos);
        } else {
            match=(c[campo].asString()==valor);
        }
        if(match) result.push_back(c);
    }
    if(result.empty()) return mkErr("Cliente nao encontrado");
    if(result.size()==1) return mkOk(result[0]);
    return mkOk(JsonValue(result));
}

inline JsonValue hCliListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string filtro=req["data"]["filtro"].asString();
    std::string fLow=filtro;
    std::transform(fLow.begin(),fLow.end(),fLow.begin(),::tolower);

    JsonValue clientes=jsonParseFile(F_CLI);
    if(!clientes.isArray()) return mkOk(JsonValue(JsonArray{}));

    if(filtro.empty()) return mkOk(clientes);

    JsonArray result;
    for(size_t i=0;i<clientes.arr.size();i++){
        std::string n=clientes.arr[i]["nome"].asString();
        std::transform(n.begin(),n.end(),n.begin(),::tolower);
        if(n.find(fLow)!=std::string::npos) result.push_back(clientes.arr[i]);
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hCliEditar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado||!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string id=d["id"].asString();

    FileLock lk;
    JsonValue clientes=jsonParseFile(F_CLI);
    if(!clientes.isArray()) return mkErr("Sem dados");

    for(size_t i=0;i<clientes.arr.size();i++){
        if(clientes.arr[i]["id"].asString()==id){
            if(d.has("nome"))     clientes.arr[i]["nome"]    =JsonValue(d["nome"].asString());
            if(d.has("telefone")) clientes.arr[i]["telefone"]=JsonValue(d["telefone"].asString());
            if(d.has("email"))    clientes.arr[i]["email"]   =JsonValue(d["email"].asString());
            if(d.has("nif"))      clientes.arr[i]["nif"]     =JsonValue(d["nif"].asString());
            jsonSaveFile(F_CLI,clientes);
            srvLog(s.username,s.loja_id,"CLI_EDITAR","id="+id);
            return mkOk(clientes.arr[i]);
        }
    }
    return mkErr("Cliente nao encontrado");
}

inline JsonValue hCliSuspender(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    std::string id=req["data"]["id"].asString();
    FileLock lk;
    JsonValue clientes=jsonParseFile(F_CLI);
    if(!clientes.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<clientes.arr.size();i++){
        if(clientes.arr[i]["id"].asString()==id){
            clientes.arr[i]["estado"]=JsonValue(std::string("suspenso"));
            jsonSaveFile(F_CLI,clientes);
            srvLog(s.username,s.loja_id,"CLI_SUSPENDER","id="+id);
            return mkOk();
        }
    }
    return mkErr("Nao encontrado");
}

inline JsonValue hCliHistorico(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string cid=req["data"]["cliente_id"].asString();

    JsonObject d;
    d["vendas"]     =JsonValue(JsonArray{});
    d["reparacoes"] =JsonValue(JsonArray{});
    d["garantias"]  =JsonValue(JsonArray{});

    JsonValue vendas=jsonParseFile(F_VND);
    if(vendas.isArray())
        for(size_t i=0;i<vendas.arr.size();i++)
            if(vendas.arr[i]["cliente_id"].asString()==cid)
                d["vendas"].arr.push_back(vendas.arr[i]);

    JsonValue reps=jsonParseFile(F_REP);
    if(reps.isArray())
        for(size_t i=0;i<reps.arr.size();i++)
            if(reps.arr[i]["cliente_id"].asString()==cid)
                d["reparacoes"].arr.push_back(reps.arr[i]);

    JsonValue gars=jsonParseFile(F_GAR);
    if(gars.isArray())
        for(size_t i=0;i<gars.arr.size();i++)
            if(gars.arr[i]["cliente_id"].asString()==cid)
                d["garantias"].arr.push_back(gars.arr[i]);

    return mkOk(JsonValue(d));
}

/* ======================================================================
 * HANDLER: PRODUTOS
 * ====================================================================== */
inline JsonValue hPrdCriar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string ean=d["ean"].asString();

    FileLock lk;
    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) prds=JsonValue(JsonArray{});

    if(!ean.empty())
        for(size_t i=0;i<prds.arr.size();i++)
            if(prds.arr[i]["ean"].asString()==ean)
                return mkErr("EAN ja existe: "+prds.arr[i]["nome"].asString());

    /* Stock por loja: objeto {"loja_id": quantidade} */
    JsonObject stock_lojas;
    if(d.has("stock_lojas")){
        for(auto it=d["stock_lojas"].obj.begin();it!=d["stock_lojas"].obj.end();++it)
            stock_lojas[it->first]=it->second;
    } else {
        /* stock inicial para a loja do utilizador */
        stock_lojas[s.loja_id]=JsonValue(d["stock"].asInt());
    }

    JsonObject p;
    p["id"]               =JsonValue(srvGenId("prd_"));
    p["nome"]             =JsonValue(d["nome"].asString());
    p["descricao"]        =JsonValue(d["descricao"].asString());
    p["preco_venda"]      =JsonValue(d["preco_venda"].asDouble());
    p["preco_custo"]      =JsonValue(d["preco_custo"].asDouble());
    p["stock_lojas"]      =JsonValue(stock_lojas);
    p["stock_minimo"]     =JsonValue(d["stock_minimo"].asInt());
    p["ean"]              =JsonValue(ean);
    p["tipo"]             =JsonValue(d["tipo"].asString()); /* produto/servico */
    p["tem_garantia"]     =JsonValue(d["tem_garantia"].asBool());
    p["duracao_garantia"] =JsonValue(d["duracao_garantia"].asInt());
    p["fornecedor_id"]    =JsonValue(d["fornecedor_id"].asString());
    p["ativo"]            =JsonValue(true);
    p["criado_em"]        =JsonValue(srvDataAtual());

    prds.arr.push_back(JsonValue(p));
    jsonSaveFile(F_PRD,prds);
    srvLog(s.username,s.loja_id,"PRD_CRIAR","nome="+d["nome"].asString());
    return mkOk(JsonValue(p));
}

inline JsonValue hPrdListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string tipo  =req["data"]["tipo"].asString();
    std::string filtro=req["data"]["filtro"].asString();
    bool stock_baixo  =req["data"]["stock_baixo"].asBool();
    std::string fLow=filtro;
    std::transform(fLow.begin(),fLow.end(),fLow.begin(),::tolower);

    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) return mkOk(JsonValue(JsonArray{}));

    JsonArray result;
    for(size_t i=0;i<prds.arr.size();i++){
        JsonValue& p=prds.arr[i];
        if(!p["ativo"].asBool()) continue;
        if(!tipo.empty() && p["tipo"].asString()!=tipo) continue;
        if(!fLow.empty()){
            std::string n=p["nome"].asString();
            std::transform(n.begin(),n.end(),n.begin(),::tolower);
            if(n.find(fLow)==std::string::npos) continue;
        }
        /* Stock da loja do utilizador */
        long long stk=0;
        if(p["stock_lojas"].isObject() && p["stock_lojas"].has(s.loja_id))
            stk=p["stock_lojas"][s.loja_id].asInt();
        if(stock_baixo && (p["tipo"].asString()!="produto" || stk>(int)p["stock_minimo"].asInt())) continue;

        JsonObject pr=p.obj;
        pr["stock_loja"]=JsonValue(stk);
        result.push_back(JsonValue(pr));
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hPrdBuscarEAN(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string ean=req["data"]["ean"].asString();
    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) return mkErr("EAN nao encontrado");
    for(size_t i=0;i<prds.arr.size();i++){
        if(prds.arr[i]["ean"].asString()==ean && prds.arr[i]["ativo"].asBool()){
            JsonObject pr=prds.arr[i].obj;
            long long stk=0;
            if(prds.arr[i]["stock_lojas"].isObject() && prds.arr[i]["stock_lojas"].has(s.loja_id))
                stk=prds.arr[i]["stock_lojas"][s.loja_id].asInt();
            pr["stock_loja"]=JsonValue(stk);
            return mkOk(JsonValue(pr));
        }
    }
    return mkErr("EAN nao encontrado");
}

inline JsonValue hPrdBuscarNome(const JsonValue& req, const SrvSessao& s) {
    return hPrdListar(req, s); /* usa o mesmo handler com filtro */
}

static bool prdReduzirStock(const std::string& prd_id, const std::string& loja_id, int qty) {
    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) return false;
    for(size_t i=0;i<prds.arr.size();i++){
        if(prds.arr[i]["id"].asString()==prd_id){
            long long atual=0;
            if(prds.arr[i]["stock_lojas"].has(loja_id))
                atual=prds.arr[i]["stock_lojas"][loja_id].asInt();
            if(atual<qty) return false;
            prds.arr[i]["stock_lojas"][loja_id]=JsonValue((long long)(atual-qty));
            jsonSaveFile(F_PRD,prds);
            return true;
        }
    }
    return false;
}

static bool prdAumentarStock(const std::string& prd_id, const std::string& loja_id, int qty) {
    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) return false;
    for(size_t i=0;i<prds.arr.size();i++){
        if(prds.arr[i]["id"].asString()==prd_id){
            long long atual=0;
            if(prds.arr[i]["stock_lojas"].has(loja_id))
                atual=prds.arr[i]["stock_lojas"][loja_id].asInt();
            prds.arr[i]["stock_lojas"][loja_id]=JsonValue((long long)(atual+qty));
            jsonSaveFile(F_PRD,prds);
            return true;
        }
    }
    return false;
}

inline JsonValue hPrdTransStock(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    std::string prd_id  =req["data"]["produto_id"].asString();
    std::string loja_org=req["data"]["loja_origem"].asString();
    std::string loja_dst=req["data"]["loja_destino"].asString();
    int qty             =(int)req["data"]["quantidade"].asInt();

    FileLock lk;
    if(!prdReduzirStock(prd_id,loja_org,qty))
        return mkErr("Stock insuficiente na loja de origem");
    prdAumentarStock(prd_id,loja_dst,qty);
    srvLog(s.username,s.loja_id,"PRD_TRANS_STOCK",
           "prd="+prd_id+" org="+loja_org+" dst="+loja_dst+" qty="+std::to_string(qty));
    return mkOk();
}

inline JsonValue hPrdEditar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string id=d["id"].asString();
    FileLock lk;
    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<prds.arr.size();i++){
        if(prds.arr[i]["id"].asString()==id){
            if(d.has("nome"))             prds.arr[i]["nome"]            =JsonValue(d["nome"].asString());
            if(d.has("descricao"))        prds.arr[i]["descricao"]       =JsonValue(d["descricao"].asString());
            if(d.has("preco_venda"))      prds.arr[i]["preco_venda"]     =JsonValue(d["preco_venda"].asDouble());
            if(d.has("preco_custo"))      prds.arr[i]["preco_custo"]     =JsonValue(d["preco_custo"].asDouble());
            if(d.has("stock_minimo"))     prds.arr[i]["stock_minimo"]    =JsonValue(d["stock_minimo"].asInt());
            if(d.has("tem_garantia"))     prds.arr[i]["tem_garantia"]    =JsonValue(d["tem_garantia"].asBool());
            if(d.has("duracao_garantia")) prds.arr[i]["duracao_garantia"]=JsonValue(d["duracao_garantia"].asInt());
            if(d.has("ativo"))            prds.arr[i]["ativo"]           =JsonValue(d["ativo"].asBool());
            if(d.has("stock_loja")){
                prds.arr[i]["stock_lojas"][s.loja_id]=JsonValue(d["stock_loja"].asInt());
            }
            jsonSaveFile(F_PRD,prds);
            srvLog(s.username,s.loja_id,"PRD_EDITAR","id="+id);
            return mkOk(prds.arr[i]);
        }
    }
    return mkErr("Produto nao encontrado");
}

/* ======================================================================
 * HANDLER: VENDAS
 * ====================================================================== */
inline JsonValue hVndCriar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado||!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];

    std::string cli_id=d["cliente_id"].asString();
    double total      =d["total"].asDouble();

    /* Verificar metodo de pagamento */
    std::string pagamento=d["pagamento"].asString();
    if(pagamento.empty()) return mkErr("Metodo de pagamento obrigatorio");

    FileLock lk;

    /* Verificar stock e itens */
    if(!d["itens"].isArray()||d["itens"].arr.empty())
        return mkErr("Sem itens na venda");

    /* Verificar stock de cada item antes de debitar */
    JsonValue prds=jsonParseFile(F_PRD);
    for(size_t i=0;i<d["itens"].arr.size();i++){
        const JsonValue& item=d["itens"].arr[i];
        if(item["tipo"].asString()!="produto") continue;
        std::string pid=item["produto_id"].asString();
        int qty=(int)item["quantidade"].asInt();
        bool ok=false;
        if(prds.isArray())
            for(size_t j=0;j<prds.arr.size();j++)
                if(prds.arr[j]["id"].asString()==pid){
                    long long stk=0;
                    if(prds.arr[j]["stock_lojas"].has(s.loja_id))
                        stk=prds.arr[j]["stock_lojas"][s.loja_id].asInt();
                    if(stk>=qty) ok=true;
                    break;
                }
        if(!ok) return mkErr("Stock insuficiente: "+item["nome"].asString());
    }

    /* Gerar numero de fatura */
    JsonValue vendas=jsonParseFile(F_VND);
    if(!vendas.isArray()) vendas=JsonValue(JsonArray{});
    int seq=srvProxSeq(vendas,"numero","FAT-");
    std::ostringstream num; num<<"FAT-"<<std::setw(6)<<std::setfill('0')<<seq;

    JsonObject v;
    v["id"]         =JsonValue(srvGenId("vnd_"));
    v["numero"]     =JsonValue(num.str());
    v["cliente_id"] =JsonValue(cli_id);
    v["loja_id"]    =JsonValue(s.loja_id);
    v["vendedor"]   =JsonValue(s.username);
    v["itens"]      =d["itens"];
    v["total"]      =JsonValue(total);
    v["pagamento"]  =JsonValue(pagamento);
    v["desconto"]   =JsonValue(d["desconto"].asDouble());
    v["data"]       =JsonValue(srvDataAtual());

    vendas.arr.push_back(JsonValue(v));
    jsonSaveFile(F_VND,vendas);

    /* Automatismos */
    std::vector<std::string> garantias_criadas;
    for(size_t i=0;i<d["itens"].arr.size();i++){
        const JsonValue& item=d["itens"].arr[i];
        /* Debitar stock */
        if(item["tipo"].asString()=="produto")
            prdReduzirStock(item["produto_id"].asString(),s.loja_id,(int)item["quantidade"].asInt());
        /* Criar garantia */
        if(item["tem_garantia"].asBool()){
            std::string df=srvGarantiaCriar(cli_id,item["nome"].asString(),
                (int)item["duracao_garantia"].asInt(),num.str(),s.loja_id);
            garantias_criadas.push_back(item["nome"].asString()+" ate "+df);
        }
    }

    /* Fidelizacao */
    srvFidAdicionar(cli_id,total);

    /* Movimento de caixa */
    {
        JsonValue cx=jsonParseFile(F_CX);
        if(cx.isArray())
            for(size_t i=0;i<cx.arr.size();i++){
                if(cx.arr[i]["loja_id"].asString()==s.loja_id &&
                   cx.arr[i]["estado"].asString()=="aberta" &&
                   cx.arr[i]["data"].asString().substr(0,10)==srvDataApenas()){
                    JsonObject mov;
                    mov["tipo"]      =JsonValue(std::string("venda"));
                    mov["referencia"]=JsonValue(num.str());
                    mov["valor"]     =JsonValue(total);
                    mov["pagamento"] =JsonValue(pagamento);
                    mov["hora"]      =JsonValue(srvDataAtual());
                    cx.arr[i]["movimentos"].arr.push_back(JsonValue(mov));
                    cx.arr[i]["total_vendas"]=JsonValue(
                        cx.arr[i]["total_vendas"].asDouble()+total);
                    jsonSaveFile(F_CX,cx);
                    break;
                }
            }
    }

    srvLog(s.username,s.loja_id,"VENDA","numero="+num.str()+" total="+std::to_string(total));

    JsonObject resp;
    resp["numero"]          =JsonValue(num.str());
    resp["garantias"]       =JsonValue(JsonArray{});
    for(auto& g:garantias_criadas)
        resp["garantias"].arr.push_back(JsonValue(g));
    return mkOk(JsonValue(resp));
}

inline JsonValue hVndListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string data_ini=req["data"]["data_ini"].asString();
    std::string data_fim=req["data"]["data_fim"].asString();

    JsonValue vendas=jsonParseFile(F_VND);
    if(!vendas.isArray()) return mkOk(JsonValue(JsonArray{}));

    JsonArray result;
    for(size_t i=0;i<vendas.arr.size();i++){
        JsonValue& v=vendas.arr[i];
        if(!temPermLoja(s,v["loja_id"].asString())) continue;
        std::string data=v["data"].asString().substr(0,10);
        if(!data_ini.empty()&&data<data_ini) continue;
        if(!data_fim.empty()&&data>data_fim) continue;
        result.push_back(v);
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hVndDetalhe(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string num=req["data"]["numero"].asString();
    JsonValue vendas=jsonParseFile(F_VND);
    if(!vendas.isArray()) return mkErr("Nao encontrado");
    for(size_t i=0;i<vendas.arr.size();i++)
        if(vendas.arr[i]["numero"].asString()==num)
            return mkOk(vendas.arr[i]);
    return mkErr("Fatura nao encontrada: "+num);
}

/* ======================================================================
 * HANDLER: DEVOLUCOES
 * ====================================================================== */
inline JsonValue hDevCriar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string num_fat=d["numero_fatura"].asString();

    /* Encontrar venda original */
    JsonValue vendas=jsonParseFile(F_VND);
    JsonValue venda_orig;
    if(vendas.isArray())
        for(size_t i=0;i<vendas.arr.size();i++)
            if(vendas.arr[i]["numero"].asString()==num_fat){
                venda_orig=vendas.arr[i]; break;
            }
    if(venda_orig.isNull()) return mkErr("Fatura nao encontrada: "+num_fat);
    if(!temPermLoja(s,venda_orig["loja_id"].asString())) return mkErr("Sem permissao para esta loja");

    FileLock lk;
    JsonValue devs=jsonParseFile(F_DEV);
    if(!devs.isArray()) devs=JsonValue(JsonArray{});
    int seq=srvProxSeq(devs,"numero","DEV-");
    std::ostringstream num; num<<"DEV-"<<std::setw(6)<<std::setfill('0')<<seq;

    /* Itens a devolver */
    double total_dev=0.0;
    if(d["itens"].isArray())
        for(size_t i=0;i<d["itens"].arr.size();i++){
            const JsonValue& item=d["itens"].arr[i];
            total_dev+=item["preco_unitario"].asDouble()*item["quantidade"].asInt();
            /* Repor stock */
            if(item["tipo"].asString()=="produto")
                prdAumentarStock(item["produto_id"].asString(),s.loja_id,(int)item["quantidade"].asInt());
        }

    JsonObject dev;
    dev["id"]             =JsonValue(srvGenId("dev_"));
    dev["numero"]         =JsonValue(num.str());
    dev["numero_fatura"]  =JsonValue(num_fat);
    dev["cliente_id"]     =JsonValue(venda_orig["cliente_id"].asString());
    dev["loja_id"]        =JsonValue(s.loja_id);
    dev["vendedor"]       =JsonValue(s.username);
    dev["itens"]          =d["itens"];
    dev["total"]          =JsonValue(total_dev);
    dev["motivo"]         =JsonValue(d["motivo"].asString());
    dev["metodo_reemb"]   =JsonValue(d["metodo_reemb"].asString());
    dev["data"]           =JsonValue(srvDataAtual());

    devs.arr.push_back(JsonValue(dev));
    jsonSaveFile(F_DEV,devs);
    srvLog(s.username,s.loja_id,"DEVOLUCAO","numero="+num.str()+" fatura="+num_fat);

    JsonObject resp; resp["numero"]=JsonValue(num.str()); resp["total"]=JsonValue(total_dev);
    return mkOk(JsonValue(resp));
}

inline JsonValue hDevListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    JsonValue devs=jsonParseFile(F_DEV);
    if(!devs.isArray()) return mkOk(JsonValue(JsonArray{}));
    JsonArray result;
    for(size_t i=0;i<devs.arr.size();i++)
        if(temPermLoja(s,devs.arr[i]["loja_id"].asString()))
            result.push_back(devs.arr[i]);
    return mkOk(JsonValue(result));
}

/* ======================================================================
 * HANDLER: ORCAMENTOS
 * ====================================================================== */
inline JsonValue hOrcCriar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado||!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];

    FileLock lk;
    JsonValue orcs=jsonParseFile(F_ORC);
    if(!orcs.isArray()) orcs=JsonValue(JsonArray{});
    int seq=srvProxSeq(orcs,"numero","ORC-");
    std::ostringstream num; num<<"ORC-"<<std::setw(6)<<std::setfill('0')<<seq;

    JsonObject o;
    o["id"]         =JsonValue(srvGenId("orc_"));
    o["numero"]     =JsonValue(num.str());
    o["cliente_id"] =JsonValue(d["cliente_id"].asString());
    o["loja_id"]    =JsonValue(s.loja_id);
    o["itens"]      =d["itens"];
    o["total"]      =JsonValue(d["total"].asDouble());
    o["estado"]     =JsonValue(std::string("pendente"));
    o["observacoes"]=JsonValue(d["observacoes"].asString());
    o["data"]       =JsonValue(srvDataAtual());
    o["criado_por"] =JsonValue(s.username);
    o["fatura_id"]  =JsonValue(std::string(""));

    orcs.arr.push_back(JsonValue(o));
    jsonSaveFile(F_ORC,orcs);
    srvLog(s.username,s.loja_id,"ORC_CRIAR","numero="+num.str());
    return mkOk(JsonValue(o));
}

inline JsonValue hOrcListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string estado=req["data"]["estado"].asString();
    JsonValue orcs=jsonParseFile(F_ORC);
    if(!orcs.isArray()) return mkOk(JsonValue(JsonArray{}));
    JsonArray result;
    for(size_t i=0;i<orcs.arr.size();i++){
        if(!temPermLoja(s,orcs.arr[i]["loja_id"].asString())) continue;
        if(!estado.empty()&&orcs.arr[i]["estado"].asString()!=estado) continue;
        result.push_back(orcs.arr[i]);
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hOrcEstado(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string num=req["data"]["numero"].asString();
    std::string estado=req["data"]["estado"].asString();
    FileLock lk;
    JsonValue orcs=jsonParseFile(F_ORC);
    if(!orcs.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<orcs.arr.size();i++){
        if(orcs.arr[i]["numero"].asString()==num){
            orcs.arr[i]["estado"]=JsonValue(estado);
            jsonSaveFile(F_ORC,orcs);
            return mkOk();
        }
    }
    return mkErr("Orcamento nao encontrado");
}

inline JsonValue hOrcConverter(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    std::string num=req["data"]["numero"].asString();
    std::string pagamento=req["data"]["pagamento"].asString();

    FileLock lk;
    JsonValue orcs=jsonParseFile(F_ORC);
    if(!orcs.isArray()) return mkErr("Sem dados");

    for(size_t i=0;i<orcs.arr.size();i++){
        if(orcs.arr[i]["numero"].asString()==num){
            if(orcs.arr[i]["estado"].asString()!="aprovado")
                return mkErr("Apenas orcamentos aprovados podem ser faturados");
            if(!orcs.arr[i]["fatura_id"].asString().empty())
                return mkErr("Ja foi faturado");

            /* Criar venda a partir do orcamento */
            JsonObject vreq_data;
            vreq_data["cliente_id"]=orcs.arr[i]["cliente_id"];
            vreq_data["itens"]     =orcs.arr[i]["itens"];
            vreq_data["total"]     =orcs.arr[i]["total"];
            vreq_data["pagamento"] =JsonValue(pagamento);
            vreq_data["desconto"]  =JsonValue(0.0);
            JsonObject vreq;
            vreq["data"]=JsonValue(vreq_data);
            JsonValue resp=hVndCriar(JsonValue(vreq),s);

            if(resp["ok"].asBool()){
                std::string num_fat=resp["data"]["numero"].asString();
                orcs.arr[i]["estado"]   =JsonValue(std::string("faturado"));
                orcs.arr[i]["fatura_id"]=JsonValue(num_fat);
                jsonSaveFile(F_ORC,orcs);
            }
            return resp;
        }
    }
    return mkErr("Orcamento nao encontrado");
}

/* ======================================================================
 * HANDLER: REPARACOES
 * ====================================================================== */
inline JsonValue hRepCriar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    const JsonValue& d=req["data"];

    FileLock lk;
    JsonValue reps=jsonParseFile(F_REP);
    if(!reps.isArray()) reps=JsonValue(JsonArray{});
    int seq=srvProxSeq(reps,"numero","REP-");
    std::ostringstream num; num<<"REP-"<<std::setw(6)<<std::setfill('0')<<seq;

    JsonObject r;
    r["id"]           =JsonValue(srvGenId("rep_"));
    r["numero"]       =JsonValue(num.str());
    r["cliente_id"]   =JsonValue(d["cliente_id"].asString());
    r["loja_id"]      =JsonValue(s.loja_id);
    r["equipamento"]  =JsonValue(d["equipamento"].asString());
    r["marca"]        =JsonValue(d["marca"].asString());
    r["serie"]        =JsonValue(d["serie"].asString());
    r["problema"]     =JsonValue(d["problema"].asString());
    r["acessorios"]   =JsonValue(d["acessorios"].asString());
    r["senha"]        =JsonValue(d["senha"].asString());
    r["estado"]       =JsonValue(std::string("recebido"));
    r["tecnico"]      =JsonValue(d["tecnico"].asString().empty()?s.username:d["tecnico"].asString());
    r["orcamento"]    =JsonValue(d["orcamento"].asDouble());
    r["valor_final"]  =JsonValue(0.0);
    r["data_entrada"] =JsonValue(srvDataAtual());
    r["data_conclusao"]=JsonValue(std::string(""));
    r["notas"]        =JsonValue(std::string(""));
    r["garantia_id"]  =JsonValue(std::string(""));

    reps.arr.push_back(JsonValue(r));
    jsonSaveFile(F_REP,reps);
    srvLog(s.username,s.loja_id,"REP_CRIAR","numero="+num.str());

    JsonObject resp; resp["numero"]=JsonValue(num.str());
    return mkOk(JsonValue(resp));
}

inline JsonValue hRepListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string estado=req["data"]["estado"].asString();
    std::string tecnico=req["data"]["tecnico"].asString();

    JsonValue reps=jsonParseFile(F_REP);
    if(!reps.isArray()) return mkOk(JsonValue(JsonArray{}));

    JsonArray result;
    for(size_t i=0;i<reps.arr.size();i++){
        JsonValue& r=reps.arr[i];
        if(!temPermLoja(s,r["loja_id"].asString())) continue;
        if(!estado.empty()&&r["estado"].asString()!=estado) continue;
        if(!tecnico.empty()&&r["tecnico"].asString()!=tecnico) continue;
        result.push_back(r);
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hRepEstado(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string num   =req["data"]["numero"].asString();
    std::string estado=req["data"]["estado"].asString();
    std::string notas =req["data"]["notas"].asString();

    FileLock lk;
    JsonValue reps=jsonParseFile(F_REP);
    if(!reps.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<reps.arr.size();i++){
        if(reps.arr[i]["numero"].asString()==num){
            reps.arr[i]["estado"]=JsonValue(estado);
            if(!notas.empty()){
                std::string n=reps.arr[i]["notas"].asString();
                reps.arr[i]["notas"]=JsonValue(n+"\n["+srvDataAtual()+"] "+notas);
            }
            jsonSaveFile(F_REP,reps);
            srvLog(s.username,s.loja_id,"REP_ESTADO","numero="+num+" estado="+estado);
            return mkOk();
        }
    }
    return mkErr("Reparacao nao encontrada");
}

inline JsonValue hRepConcluir(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    const JsonValue& d=req["data"];
    std::string num=d["numero"].asString();

    FileLock lk;
    JsonValue reps=jsonParseFile(F_REP);
    if(!reps.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<reps.arr.size();i++){
        if(reps.arr[i]["numero"].asString()==num){
            double val=d["valor_final"].asDouble();
            reps.arr[i]["estado"]        =JsonValue(std::string("concluido"));
            reps.arr[i]["data_conclusao"]=JsonValue(srvDataAtual());
            reps.arr[i]["valor_final"]   =JsonValue(val);
            std::string notas=reps.arr[i]["notas"].asString();
            notas+="\n[CONCLUSAO "+srvDataAtual()+"] "+d["diagnostico"].asString();
            if(!d["pecas"].asString().empty()) notas+=" | Pecas: "+d["pecas"].asString();
            reps.arr[i]["notas"]=JsonValue(notas);

            /* Garantia automatica 30 dias */
            std::string equip=reps.arr[i]["equipamento"].asString()+" "+reps.arr[i]["marca"].asString();
            std::string gid=srvGarantiaCriar(reps.arr[i]["cliente_id"].asString(),
                "Reparacao: "+equip,30,num,s.loja_id);
            reps.arr[i]["garantia_id"]=JsonValue(gid);
            jsonSaveFile(F_REP,reps);
            srvLog(s.username,s.loja_id,"REP_CONCLUIR","numero="+num+" valor="+std::to_string(val));

            JsonObject resp;
            resp["garantia_ate"]=JsonValue(srvAdicionarDias(srvDataApenas(),30));
            return mkOk(JsonValue(resp));
        }
    }
    return mkErr("Reparacao nao encontrada");
}

/* ======================================================================
 * HANDLER: GARANTIAS
 * ====================================================================== */
inline JsonValue hGarListar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string cli_id=req["data"]["cliente_id"].asString();
    JsonValue gars=jsonParseFile(F_GAR);
    if(!gars.isArray()) return mkOk(JsonValue(JsonArray{}));
    JsonArray result;
    for(size_t i=0;i<gars.arr.size();i++){
        if(!cli_id.empty()&&gars.arr[i]["cliente_id"].asString()!=cli_id) continue;
        if(!temPermLoja(s,gars.arr[i]["loja_id"].asString())) continue;
        result.push_back(gars.arr[i]);
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hGarVerificar(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string campo=req["data"]["campo"].asString(); /* nif ou telefone */
    std::string valor=req["data"]["valor"].asString();

    /* Encontrar cliente */
    JsonValue clientes=jsonParseFile(F_CLI);
    std::string cli_id;
    if(clientes.isArray())
        for(size_t i=0;i<clientes.arr.size();i++)
            if(clientes.arr[i][campo].asString()==valor){
                cli_id=clientes.arr[i]["id"].asString(); break;
            }
    if(cli_id.empty()) return mkErr("Cliente nao encontrado");

    JsonObject rr; rr["cliente_id"]=JsonValue(cli_id);
    JsonObject rreq; rreq["data"]=JsonValue(rr);
    return hGarListar(JsonValue(rreq),s);
}

/* ======================================================================
 * HANDLER: CAIXA
 * ====================================================================== */
inline JsonValue hCxAbrir(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    double fundo=req["data"]["fundo"].asDouble();

    FileLock lk;
    JsonValue cx=jsonParseFile(F_CX);
    if(!cx.isArray()) cx=JsonValue(JsonArray{});

    /* Verificar se ja existe caixa aberta hoje nesta loja */
    std::string hoje=srvDataApenas();
    for(size_t i=0;i<cx.arr.size();i++)
        if(cx.arr[i]["loja_id"].asString()==s.loja_id &&
           cx.arr[i]["data"].asString()==hoje &&
           cx.arr[i]["estado"].asString()=="aberta")
            return mkErr("Caixa ja esta aberta hoje nesta loja");

    JsonObject c;
    c["id"]           =JsonValue(srvGenId("cx_"));
    c["loja_id"]      =JsonValue(s.loja_id);
    c["data"]         =JsonValue(hoje);
    c["hora_abertura"]=JsonValue(srvDataAtual());
    c["hora_fecho"]   =JsonValue(std::string(""));
    c["aberto_por"]   =JsonValue(s.username);
    c["fechado_por"]  =JsonValue(std::string(""));
    c["fundo"]        =JsonValue(fundo);
    c["total_vendas"] =JsonValue(0.0);
    c["total_devolucoes"]=JsonValue(0.0);
    c["estado"]       =JsonValue(std::string("aberta"));
    c["movimentos"]   =JsonValue(JsonArray{});

    cx.arr.push_back(JsonValue(c));
    jsonSaveFile(F_CX,cx);
    srvLog(s.username,s.loja_id,"CX_ABRIR","fundo="+std::to_string(fundo));
    return mkOk();
}

inline JsonValue hCxFechar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    double contagem=req["data"]["contagem"].asDouble();

    FileLock lk;
    JsonValue cx=jsonParseFile(F_CX);
    if(!cx.isArray()) return mkErr("Sem dados de caixa");

    std::string hoje=srvDataApenas();
    for(size_t i=0;i<cx.arr.size();i++){
        if(cx.arr[i]["loja_id"].asString()==s.loja_id &&
           cx.arr[i]["data"].asString()==hoje &&
           cx.arr[i]["estado"].asString()=="aberta"){
            double fundo      =cx.arr[i]["fundo"].asDouble();
            double tot_vendas =cx.arr[i]["total_vendas"].asDouble();
            double esperado   =fundo+tot_vendas;
            double diferenca  =contagem-esperado;

            cx.arr[i]["estado"]     =JsonValue(std::string("fechada"));
            cx.arr[i]["hora_fecho"] =JsonValue(srvDataAtual());
            cx.arr[i]["fechado_por"]=JsonValue(s.username);
            cx.arr[i]["contagem"]   =JsonValue(contagem);
            cx.arr[i]["esperado"]   =JsonValue(esperado);
            cx.arr[i]["diferenca"]  =JsonValue(diferenca);
            jsonSaveFile(F_CX,cx);
            srvLog(s.username,s.loja_id,"CX_FECHAR","diferenca="+std::to_string(diferenca));

            JsonObject resp;
            resp["fundo"]     =JsonValue(fundo);
            resp["vendas"]    =JsonValue(tot_vendas);
            resp["esperado"]  =JsonValue(esperado);
            resp["contagem"]  =JsonValue(contagem);
            resp["diferenca"] =JsonValue(diferenca);
            return mkOk(JsonValue(resp));
        }
    }
    return mkErr("Sem caixa aberta hoje nesta loja");
}

inline JsonValue hCxEstado(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    JsonValue cx=jsonParseFile(F_CX);
    std::string hoje=srvDataApenas();
    if(cx.isArray())
        for(size_t i=0;i<cx.arr.size();i++)
            if(cx.arr[i]["loja_id"].asString()==s.loja_id &&
               cx.arr[i]["data"].asString()==hoje &&
               cx.arr[i]["estado"].asString()=="aberta")
                return mkOk(cx.arr[i]);
    JsonObject d; d["estado"]=JsonValue(std::string("fechada")); d["hoje"]=JsonValue(hoje);
    return mkOk(JsonValue(d));
}

inline JsonValue hCxMovimentos(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    JsonValue cx=jsonParseFile(F_CX);
    std::string hoje=srvDataApenas();
    if(cx.isArray())
        for(size_t i=0;i<cx.arr.size();i++)
            if(cx.arr[i]["loja_id"].asString()==s.loja_id &&
               cx.arr[i]["data"].asString()==hoje)
                return mkOk(cx.arr[i]["movimentos"]);
    return mkOk(JsonValue(JsonArray{}));
}

/* ======================================================================
 * HANDLER: FORNECEDORES
 * ====================================================================== */
inline JsonValue hFornCriar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    FileLock lk;
    JsonValue forns=jsonParseFile(F_FORN);
    if(!forns.isArray()) forns=JsonValue(JsonArray{});

    JsonObject f;
    f["id"]      =JsonValue(srvGenId("forn_"));
    f["nome"]    =JsonValue(d["nome"].asString());
    f["nif"]     =JsonValue(d["nif"].asString());
    f["email"]   =JsonValue(d["email"].asString());
    f["telefone"]=JsonValue(d["telefone"].asString());
    f["morada"]  =JsonValue(d["morada"].asString());
    f["ativo"]   =JsonValue(true);
    f["criado_em"]=JsonValue(srvDataAtual());

    forns.arr.push_back(JsonValue(f));
    jsonSaveFile(F_FORN,forns);
    srvLog(s.username,s.loja_id,"FORN_CRIAR","nome="+d["nome"].asString());
    return mkOk(JsonValue(f));
}

inline JsonValue hFornListar(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    return mkOk(jsonParseFile(F_FORN));
}

inline JsonValue hFornEncomenda(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    srvLog(s.username,s.loja_id,"FORN_ENCOMENDA",
           "forn="+d["fornecedor_id"].asString()+" prd="+d["produto_id"].asString()+
           " qty="+d["quantidade"].asString());
    /* Registo simples - integracao com fornecedor e manual */
    return mkOk();
}

/* ======================================================================
 * HANDLER: RELATORIOS
 * ====================================================================== */
inline JsonValue hRelVendas(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    std::string data_ini=req["data"]["data_ini"].asString();
    std::string data_fim=req["data"]["data_fim"].asString();
    std::string loja_id =req["data"]["loja_id"].asString();
    if(!loja_id.empty()&&!temPermLoja(s,loja_id)) return mkErr("Sem permissao para esta loja");

    JsonValue vendas=jsonParseFile(F_VND);
    if(!vendas.isArray()) return mkOk(JsonValue(JsonArray{}));

    double total=0; int count=0;
    std::map<std::string,double> por_loja, por_vendedor, por_pagamento;
    JsonArray lista;

    for(size_t i=0;i<vendas.arr.size();i++){
        JsonValue& v=vendas.arr[i];
        if(!temPermLoja(s,v["loja_id"].asString())) continue;
        if(!loja_id.empty()&&v["loja_id"].asString()!=loja_id) continue;
        std::string data=v["data"].asString().substr(0,10);
        if(!data_ini.empty()&&data<data_ini) continue;
        if(!data_fim.empty()&&data>data_fim) continue;
        double t=v["total"].asDouble();
        total+=t; count++;
        por_loja[v["loja_id"].asString()]+=t;
        por_vendedor[v["vendedor"].asString()]+=t;
        por_pagamento[v["pagamento"].asString()]+=t;
        lista.push_back(v);
    }

    JsonObject d;
    d["total"]   =JsonValue(total);
    d["count"]   =JsonValue((long long)count);
    d["vendas"]  =JsonValue(lista);

    /* Por loja */
    JsonObject lj;
    for(auto it=por_loja.begin();it!=por_loja.end();++it) lj[it->first]=JsonValue(it->second);
    d["por_loja"]=JsonValue(lj);

    /* Por vendedor */
    JsonObject vd;
    for(auto it=por_vendedor.begin();it!=por_vendedor.end();++it) vd[it->first]=JsonValue(it->second);
    d["por_vendedor"]=JsonValue(vd);

    /* Por pagamento */
    JsonObject pg;
    for(auto it=por_pagamento.begin();it!=por_pagamento.end();++it) pg[it->first]=JsonValue(it->second);
    d["por_pagamento"]=JsonValue(pg);

    return mkOk(JsonValue(d));
}

inline JsonValue hRelStock(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    JsonValue prds=jsonParseFile(F_PRD);
    if(!prds.isArray()) return mkOk(JsonValue(JsonArray{}));
    JsonArray baixo, zerado;
    for(size_t i=0;i<prds.arr.size();i++){
        JsonValue& p=prds.arr[i];
        if(!p["ativo"].asBool()||p["tipo"].asString()!="produto") continue;
        long long stk=0;
        if(p["stock_lojas"].has(s.loja_id)) stk=p["stock_lojas"][s.loja_id].asInt();
        if(stk==0)    zerado.push_back(p);
        else if(stk<=(int)p["stock_minimo"].asInt()) baixo.push_back(p);
    }
    JsonObject d; d["baixo"]=JsonValue(baixo); d["zerado"]=JsonValue(zerado);
    return mkOk(JsonValue(d));
}

inline JsonValue hRelTopProdutos(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    std::string data_ini=req["data"]["data_ini"].asString();
    std::string data_fim=req["data"]["data_fim"].asString();

    JsonValue vendas=jsonParseFile(F_VND);
    std::map<std::string,std::pair<int,double>> contagem; /* id -> {qtd, total} */

    if(vendas.isArray())
        for(size_t i=0;i<vendas.arr.size();i++){
            if(!temPermLoja(s,vendas.arr[i]["loja_id"].asString())) continue;
            std::string data=vendas.arr[i]["data"].asString().substr(0,10);
            if(!data_ini.empty()&&data<data_ini) continue;
            if(!data_fim.empty()&&data>data_fim) continue;
            if(vendas.arr[i]["itens"].isArray())
                for(size_t j=0;j<vendas.arr[i]["itens"].arr.size();j++){
                    const JsonValue& item=vendas.arr[i]["itens"].arr[j];
                    std::string nome=item["nome"].asString();
                    contagem[nome].first +=(int)item["quantidade"].asInt();
                    contagem[nome].second+=item["subtotal"].asDouble();
                }
        }

    /* Ordenar por quantidade */
    std::vector<std::pair<std::string,std::pair<int,double>>> sorted(contagem.begin(),contagem.end());
    std::sort(sorted.begin(),sorted.end(),[](const std::pair<std::string,std::pair<int,double>>& a,
              const std::pair<std::string,std::pair<int,double>>& b){return a.second.first>b.second.first;});

    JsonArray result;
    for(size_t i=0;i<sorted.size()&&i<20;i++){
        JsonObject r;
        r["nome"]=JsonValue(sorted[i].first);
        r["quantidade"]=JsonValue((long long)sorted[i].second.first);
        r["total"]=JsonValue(sorted[i].second.second);
        result.push_back(JsonValue(r));
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hRelVendedor(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    std::string data_ini=req["data"]["data_ini"].asString();
    std::string data_fim=req["data"]["data_fim"].asString();

    JsonValue vendas=jsonParseFile(F_VND);
    std::map<std::string,std::pair<int,double>> stats;

    if(vendas.isArray())
        for(size_t i=0;i<vendas.arr.size();i++){
            if(!temPermLoja(s,vendas.arr[i]["loja_id"].asString())) continue;
            std::string data=vendas.arr[i]["data"].asString().substr(0,10);
            if(!data_ini.empty()&&data<data_ini) continue;
            if(!data_fim.empty()&&data>data_fim) continue;
            std::string vnd=vendas.arr[i]["vendedor"].asString();
            stats[vnd].first++;
            stats[vnd].second+=vendas.arr[i]["total"].asDouble();
        }

    JsonArray result;
    for(auto it=stats.begin();it!=stats.end();++it){
        JsonObject r;
        r["vendedor"]=JsonValue(it->first);
        r["vendas"]  =JsonValue((long long)it->second.first);
        r["total"]   =JsonValue(it->second.second);
        result.push_back(JsonValue(r));
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hRelReparacoes(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    std::string estado=req["data"]["estado"].asString();
    JsonValue reps=jsonParseFile(F_REP);
    if(!reps.isArray()) return mkOk(JsonValue(JsonArray{}));
    JsonArray result;
    double total=0;
    for(size_t i=0;i<reps.arr.size();i++){
        if(!temPermLoja(s,reps.arr[i]["loja_id"].asString())) continue;
        if(!estado.empty()&&reps.arr[i]["estado"].asString()!=estado) continue;
        result.push_back(reps.arr[i]);
        total+=reps.arr[i]["valor_final"].asDouble();
    }
    JsonObject d; d["reparacoes"]=JsonValue(result); d["total_faturado"]=JsonValue(total);
    return mkOk(JsonValue(d));
}

/* ======================================================================
 * HANDLER: UTILIZADORES (admin)
 * ====================================================================== */
inline JsonValue hUsrCriar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"admin")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string user=d["username"].asString();

    /* Validar username: exatamente 3 letras */
    if(user.size()!=3) return mkErr("Username deve ter exatamente 3 letras");
    for(char c:user) if(!std::isalpha(c)) return mkErr("Username so pode ter letras");

    FileLock lk;
    JsonValue utils=jsonParseFile(F_UTIL);
    if(!utils.isArray()) utils=JsonValue(JsonArray{});
    for(size_t i=0;i<utils.arr.size();i++)
        if(utils.arr[i]["username"].asString()==user) return mkErr("Username ja existe");

    JsonObject u;
    u["id"]       =JsonValue(srvGenId("usr_"));
    u["username"] =JsonValue(user);
    u["password"] =JsonValue(d["password"].asString()); /* ja em SHA-256 */
    u["nome"]     =JsonValue(d["nome"].asString());
    u["role"]     =JsonValue(d["role"].asString());
    u["loja_id"]  =JsonValue(d["loja_id"].asString());
    u["estado"]   =JsonValue(std::string("ativo"));
    u["criado_em"]=JsonValue(srvDataAtual());

    utils.arr.push_back(JsonValue(u));
    jsonSaveFile(F_UTIL,utils);
    srvLog(s.username,s.loja_id,"USR_CRIAR","username="+user+" role="+d["role"].asString());
    return mkOk();
}

inline JsonValue hUsrListar(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    JsonValue utils=jsonParseFile(F_UTIL);
    if(!utils.isArray()) return mkOk(JsonValue(JsonArray{}));
    /* Remover passwords da resposta */
    JsonArray result;
    for(size_t i=0;i<utils.arr.size();i++){
        JsonObject u=utils.arr[i].obj;
        u.erase("password");
        result.push_back(JsonValue(u));
    }
    return mkOk(JsonValue(result));
}

inline JsonValue hUsrEditar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"admin")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string user=d["username"].asString();
    FileLock lk;
    JsonValue utils=jsonParseFile(F_UTIL);
    if(!utils.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<utils.arr.size();i++){
        if(utils.arr[i]["username"].asString()==user){
            if(d.has("role"))     utils.arr[i]["role"]    =JsonValue(d["role"].asString());
            if(d.has("estado"))   utils.arr[i]["estado"]  =JsonValue(d["estado"].asString());
            if(d.has("loja_id"))  utils.arr[i]["loja_id"] =JsonValue(d["loja_id"].asString());
            if(d.has("password")) utils.arr[i]["password"]=JsonValue(d["password"].asString());
            if(d.has("nome"))     utils.arr[i]["nome"]    =JsonValue(d["nome"].asString());
            jsonSaveFile(F_UTIL,utils);
            srvLog(s.username,s.loja_id,"USR_EDITAR","username="+user);
            return mkOk();
        }
    }
    return mkErr("Utilizador nao encontrado");
}

/* ======================================================================
 * HANDLER: LOJAS
 * ====================================================================== */
inline JsonValue hLojCriar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"admin")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    FileLock lk;
    JsonValue lojas=jsonParseFile(F_LOJ);
    if(!lojas.isArray()) lojas=JsonValue(JsonArray{});

    JsonObject l;
    l["id"]         =JsonValue(srvGenId("loj_"));
    l["nome"]       =JsonValue(d["nome"].asString());
    l["localizacao"]=JsonValue(d["localizacao"].asString());
    l["telefone"]   =JsonValue(d["telefone"].asString());
    l["estado"]     =JsonValue(std::string("ativo"));
    l["criado_em"]  =JsonValue(srvDataAtual());

    lojas.arr.push_back(JsonValue(l));
    jsonSaveFile(F_LOJ,lojas);
    srvLog(s.username,s.loja_id,"LOJ_CRIAR","nome="+d["nome"].asString());
    return mkOk(JsonValue(l));
}

inline JsonValue hLojListar(const JsonValue& /*req*/, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    return mkOk(jsonParseFile(F_LOJ));
}

inline JsonValue hLojEditar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"admin")) return mkErr("Sem permissao");
    const JsonValue& d=req["data"];
    std::string id=d["id"].asString();
    FileLock lk;
    JsonValue lojas=jsonParseFile(F_LOJ);
    if(!lojas.isArray()) return mkErr("Sem dados");
    for(size_t i=0;i<lojas.arr.size();i++){
        if(lojas.arr[i]["id"].asString()==id){
            if(d.has("nome"))        lojas.arr[i]["nome"]       =JsonValue(d["nome"].asString());
            if(d.has("localizacao")) lojas.arr[i]["localizacao"]=JsonValue(d["localizacao"].asString());
            if(d.has("telefone"))    lojas.arr[i]["telefone"]   =JsonValue(d["telefone"].asString());
            if(d.has("estado"))      lojas.arr[i]["estado"]     =JsonValue(d["estado"].asString());
            jsonSaveFile(F_LOJ,lojas);
            return mkOk();
        }
    }
    return mkErr("Loja nao encontrada");
}

/* ======================================================================
 * HANDLER: FIDELIZACAO
 * ====================================================================== */
inline JsonValue hFidSaldo(const JsonValue& req, const SrvSessao& s) {
    if(!s.autenticado) return mkErr("Nao autenticado");
    std::string cli_id=req["data"]["cliente_id"].asString();
    JsonValue fid=jsonParseFile(F_FID);
    if(fid.isArray())
        for(size_t i=0;i<fid.arr.size();i++)
            if(fid.arr[i]["cliente_id"].asString()==cli_id)
                return mkOk(fid.arr[i]);
    JsonObject d; d["cliente_id"]=JsonValue(cli_id); d["pontos"]=JsonValue((long long)0);
    return mkOk(JsonValue(d));
}

inline JsonValue hFidResgatar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"vendedor")) return mkErr("Sem permissao");
    std::string cli_id=req["data"]["cliente_id"].asString();
    int pts=(int)req["data"]["pontos"].asInt();

    FileLock lk;
    JsonValue fid=jsonParseFile(F_FID);
    if(!fid.isArray()) return mkErr("Sem pontos");
    for(size_t i=0;i<fid.arr.size();i++){
        if(fid.arr[i]["cliente_id"].asString()==cli_id){
            int atual=(int)fid.arr[i]["pontos"].asInt();
            if(atual<pts) return mkErr("Pontos insuficientes (tem "+std::to_string(atual)+")");
            fid.arr[i]["pontos"]=JsonValue((long long)(atual-pts));
            jsonSaveFile(F_FID,fid);
            double desconto=pts*0.5; /* 1 ponto = 0.50 EUR de desconto */
            JsonObject d; d["pontos_usados"]=JsonValue((long long)pts); d["desconto_eur"]=JsonValue(desconto);
            return mkOk(JsonValue(d));
        }
    }
    return mkErr("Cliente sem pontos registados");
}

/* ======================================================================
 * HANDLER: LOGS
 * ====================================================================== */
inline JsonValue hLogListar(const JsonValue& req, const SrvSessao& s) {
    if(!temPerm(s,"gerente")) return mkErr("Sem permissao");
    int limite=(int)req["data"]["limite"].asInt();
    if(limite<=0) limite=100;
    JsonValue logs=jsonParseFile(F_LOG);
    if(!logs.isArray()) return mkOk(JsonValue(JsonArray{}));
    int total=(int)logs.arr.size();
    int inicio=total>limite?total-limite:0;
    JsonArray result;
    for(int i=inicio;i<total;i++){
        if(!temPermLoja(s,logs.arr[i]["loja_id"].asString())) continue;
        result.push_back(logs.arr[i]);
    }
    return mkOk(JsonValue(result));
}

/* ======================================================================
 * DISPATCHER — mapeia CMD para handler
 * ====================================================================== */
inline JsonValue srvDispatch(const std::string& cmd, const JsonValue& req, SrvSessao& sessao) {
    /* Auth */
    if(cmd==CMD_LOGIN)           return hLogin(req,sessao);
    if(!sessao.autenticado)      return mkErr("Nao autenticado");

    /* Dashboard */
    if(cmd==CMD_DASHBOARD)       return hDashboard(req,sessao);

    /* Clientes */
    if(cmd==CMD_CLI_CRIAR)       return hCliCriar(req,sessao);
    if(cmd==CMD_CLI_BUSCAR)      return hCliBuscar(req,sessao);
    if(cmd==CMD_CLI_LISTAR)      return hCliListar(req,sessao);
    if(cmd==CMD_CLI_EDITAR)      return hCliEditar(req,sessao);
    if(cmd==CMD_CLI_SUSPENDER)   return hCliSuspender(req,sessao);
    if(cmd==CMD_CLI_HISTORICO)   return hCliHistorico(req,sessao);

    /* Produtos */
    if(cmd==CMD_PRD_CRIAR)       return hPrdCriar(req,sessao);
    if(cmd==CMD_PRD_LISTAR)      return hPrdListar(req,sessao);
    if(cmd==CMD_PRD_BUSCAR_EAN)  return hPrdBuscarEAN(req,sessao);
    if(cmd==CMD_PRD_BUSCAR_NOME) return hPrdBuscarNome(req,sessao);
    if(cmd==CMD_PRD_EDITAR)      return hPrdEditar(req,sessao);
    if(cmd==CMD_PRD_STOCK_BAIXO) { JsonObject d; d["stock_baixo"]=JsonValue(true); JsonObject r; r["data"]=JsonValue(d); return hPrdListar(JsonValue(r),sessao); }
    if(cmd==CMD_PRD_TRANS_STOCK) return hPrdTransStock(req,sessao);

    /* Vendas */
    if(cmd==CMD_VND_CRIAR)       return hVndCriar(req,sessao);
    if(cmd==CMD_VND_LISTAR)      return hVndListar(req,sessao);
    if(cmd==CMD_VND_DETALHE)     return hVndDetalhe(req,sessao);

    /* Devolucoes */
    if(cmd==CMD_DEV_CRIAR)       return hDevCriar(req,sessao);
    if(cmd==CMD_DEV_LISTAR)      return hDevListar(req,sessao);

    /* Orcamentos */
    if(cmd==CMD_ORC_CRIAR)       return hOrcCriar(req,sessao);
    if(cmd==CMD_ORC_LISTAR)      return hOrcListar(req,sessao);
    if(cmd==CMD_ORC_ESTADO)      return hOrcEstado(req,sessao);
    if(cmd==CMD_ORC_CONVERTER)   return hOrcConverter(req,sessao);

    /* Reparacoes */
    if(cmd==CMD_REP_CRIAR)       return hRepCriar(req,sessao);
    if(cmd==CMD_REP_LISTAR)      return hRepListar(req,sessao);
    if(cmd==CMD_REP_ESTADO)      return hRepEstado(req,sessao);
    if(cmd==CMD_REP_CONCLUIR)    return hRepConcluir(req,sessao);

    /* Garantias */
    if(cmd==CMD_GAR_LISTAR)      return hGarListar(req,sessao);
    if(cmd==CMD_GAR_VERIFICAR)   return hGarVerificar(req,sessao);

    /* Caixa */
    if(cmd==CMD_CX_ABRIR)        return hCxAbrir(req,sessao);
    if(cmd==CMD_CX_FECHAR)       return hCxFechar(req,sessao);
    if(cmd==CMD_CX_ESTADO)       return hCxEstado(req,sessao);
    if(cmd==CMD_CX_MOVIMENTOS)   return hCxMovimentos(req,sessao);

    /* Fornecedores */
    if(cmd==CMD_FORN_CRIAR)      return hFornCriar(req,sessao);
    if(cmd==CMD_FORN_LISTAR)     return hFornListar(req,sessao);
    if(cmd==CMD_FORN_ENCOMENDA)  return hFornEncomenda(req,sessao);

    /* Relatorios */
    if(cmd==CMD_REL_VENDAS)      return hRelVendas(req,sessao);
    if(cmd==CMD_REL_STOCK)       return hRelStock(req,sessao);
    if(cmd==CMD_REL_TOP_PRODUTOS)return hRelTopProdutos(req,sessao);
    if(cmd==CMD_REL_VENDEDOR)    return hRelVendedor(req,sessao);
    if(cmd==CMD_REL_REPARACOES)  return hRelReparacoes(req,sessao);

    /* Utilizadores */
    if(cmd==CMD_USR_CRIAR)       return hUsrCriar(req,sessao);
    if(cmd==CMD_USR_LISTAR)      return hUsrListar(req,sessao);
    if(cmd==CMD_USR_EDITAR)      return hUsrEditar(req,sessao);

    /* Lojas */
    if(cmd==CMD_LOJ_CRIAR)       return hLojCriar(req,sessao);
    if(cmd==CMD_LOJ_LISTAR)      return hLojListar(req,sessao);
    if(cmd==CMD_LOJ_EDITAR)      return hLojEditar(req,sessao);

    /* Fidelizacao */
    if(cmd==CMD_FID_SALDO)       return hFidSaldo(req,sessao);
    if(cmd==CMD_FID_RESGATAR)    return hFidResgatar(req,sessao);

    /* Logs */
    if(cmd==CMD_LOG_LISTAR)      return hLogListar(req,sessao);

    return mkErr("Comando desconhecido: "+cmd);
}
