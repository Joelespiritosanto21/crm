#pragma once
/*
 * common.h - TUI de ecra fixo 80 colunas com bordas
 *
 *  ╔══════════════════════════════════════════════════════════════════════════════╗
 *  ║  TECHFIX │ TITULO DO ECRA                         adm [admin]  14:32:01   ║
 *  ╠══════════════════════════════════════════════════════════════════════════════╣
 *  ║  conteudo...                                                               ║
 *  ╠══════════════════════════════════════════════════════════════════════════════╣
 *  ║  Menu > Seccao                                       [ 0=Voltar  Enter ]  ║
 *  ╚══════════════════════════════════════════════════════════════════════════════╝
 */

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <limits>
#include <vector>
#include <cstdio>
#include <cstring>

#define UI_W   80
#define UI_IW  78

#define DATA_DIR           "data/"
#define DOCS_DIR           "docs/"
#define FILE_UTILIZADORES  DATA_DIR "utilizadores.json"
#define FILE_CLIENTES      DATA_DIR "clientes.json"
#define FILE_PRODUTOS      DATA_DIR "produtos.json"
#define FILE_VENDAS        DATA_DIR "vendas.json"
#define FILE_ORCAMENTOS    DATA_DIR "orcamentos.json"
#define FILE_REPARACOES    DATA_DIR "reparacoes.json"
#define FILE_GARANTIAS     DATA_DIR "garantias.json"
#define FILE_LOJAS         DATA_DIR "lojas.json"
#define FILE_LOGS          DATA_DIR "logs.json"

struct Sessao {
    std::string username;
    std::string role;
    std::string loja_id;
    bool autenticado;
    Sessao() : autenticado(false) {}
};
extern Sessao       g_sessao;
extern std::string  g_breadcrumb;
extern std::string  g_dica;

#ifdef _WIN32
  #include <windows.h>
  inline void uiInitConsole() {
      SetConsoleOutputCP(65001);
      SetConsoleCP(65001);
  }
#else
  inline void uiInitConsole() {}
#endif

/* ── Limpar ecra ─────────────────────────────────────────────── */
inline void cls() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
    std::cout.flush();
}

/* ── Strings com largura fixa ────────────────────────────────── */
inline std::string fw(const std::string& s, int w, char fill=' ') {
    std::string r=s;
    if((int)r.size()>w) r=r.substr(0,w);
    while((int)r.size()<w) r+=fill;
    return r;
}
inline std::string fwR(const std::string& s, int w, char fill=' ') {
    std::string r=s;
    if((int)r.size()>w) r=r.substr(0,w);
    while((int)r.size()<w) r=fill+r;
    return r;
}
inline std::string fwL(const std::string& s, int w, char fill=' ') {
    return fwR(s,w,fill);
}

/* ── Box-drawing UTF-8 ───────────────────────────────────────── */
static const char* BX_TL = "\xe2\x95\x94";
static const char* BX_TR = "\xe2\x95\x97";
static const char* BX_BL = "\xe2\x95\x9a";
static const char* BX_BR = "\xe2\x95\x9d";
static const char* BX_H  = "\xe2\x95\x90";
static const char* BX_V  = "\xe2\x95\x91";
static const char* BX_ML = "\xe2\x95\xa0";
static const char* BX_MR = "\xe2\x95\xa3";

static inline std::string repH(int n) {
    std::string r;
    for(int i=0;i<n;i++) r+=BX_H;
    return r;
}

/* contar caracteres visiveis em UTF-8 */
static inline int visLen(const std::string& s) {
    int v=0;
    for(size_t i=0;i<s.size();) {
        unsigned char b=(unsigned char)s[i];
        if      (b<0x80) { ++v; ++i; }
        else if (b<0xE0) { ++v; i+=2; }
        else if (b<0xF0) { ++v; i+=3; }
        else             { ++v; i+=4; }
    }
    return v;
}

/* linha ╔════╗ */
static inline std::string topLine()  { return std::string(BX_TL)+repH(UI_W-2)+BX_TR+"\n"; }
static inline std::string botLine()  { return std::string(BX_BL)+repH(UI_W-2)+BX_BR+"\n"; }
static inline std::string sepLine()  { return std::string(BX_ML)+repH(UI_W-2)+BX_MR+"\n"; }

/* linha de conteudo ║ texto (pad) ║ */
static inline std::string boxL(const std::string& content) {
    int vl = visLen(content);
    int pad = UI_IW - vl;
    if(pad<0) {
        /* truncar */
        std::string r; int vis2=0;
        for(size_t i=0;i<content.size();) {
            unsigned char b=(unsigned char)content[i];
            int w=(b<0x80)?1:(b<0xE0)?2:(b<0xF0)?3:4;
            if(vis2+1>UI_IW) break;
            r+=content.substr(i,w); vis2++; i+=w;
        }
        return std::string(BX_V)+r+std::string(UI_IW-vis2,' ')+BX_V+"\n";
    }
    return std::string(BX_V)+content+std::string(pad,' ')+BX_V+"\n";
}

/* ── Datas ───────────────────────────────────────────────────── */
inline std::string dataAtual() {
    std::time_t t=std::time(0); char b[32];
    std::strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",std::localtime(&t));
    return b;
}
inline std::string dataApenasData() {
    std::time_t t=std::time(0); char b[16];
    std::strftime(b,sizeof(b),"%Y-%m-%d",std::localtime(&t));
    return b;
}
inline std::string horaAtual() {
    std::time_t t=std::time(0); char b[10];
    std::strftime(b,sizeof(b),"%H:%M:%S",std::localtime(&t));
    return b;
}
inline std::string adicionarDias(const std::string& ds, int dias) {
    struct tm tm2; memset(&tm2,0,sizeof(tm2));
    int y=0,mo=0,d=0;
    sscanf(ds.c_str(),"%d-%d-%d",&y,&mo,&d);
    tm2.tm_year=y-1900; tm2.tm_mon=mo-1; tm2.tm_mday=d;
    std::time_t t=std::mktime(&tm2); t+=(std::time_t)dias*86400;
    char buf[16]; std::strftime(buf,sizeof(buf),"%Y-%m-%d",std::localtime(&t));
    return buf;
}

/* ── Layout de ecra ─────────────────────────────────────────── */
inline void iniciarEcra(const std::string& titulo,
                        const std::string& breadcrumb="",
                        const std::string& dica="") {
    if(!breadcrumb.empty()) g_breadcrumb=breadcrumb;
    if(!dica.empty())       g_dica=dica;
    cls();
    std::cout << topLine();
    /* cabecalho */
    std::string hora = horaAtual();
    std::string ui;
    if(g_sessao.autenticado)
        ui=g_sessao.username+" ["+g_sessao.role+"]  "+hora;
    else ui=hora;
    std::string esq=" TECHFIX \xe2\x94\x82 "+titulo;
    int sp=UI_IW-(int)esq.size()-(int)ui.size()-1;
    if(sp<1) sp=1;
    std::cout << boxL(esq+std::string(sp,' ')+ui+" ");
    std::cout << sepLine();
    /* linha vazia */
    std::cout << boxL("");
}

inline void fecharEcra() {
    std::cout << boxL("");
    std::cout << sepLine();
    std::string esq=" "+g_breadcrumb;
    std::string dir;
    if(!g_dica.empty()) dir="[ "+g_dica+" ] ";
    int sp=UI_IW-(int)esq.size()-(int)dir.size();
    if(sp<1) sp=1;
    std::cout << boxL(esq+std::string(sp,' ')+dir);
    std::cout << botLine();
}

/* Imprimir linha dentro do ecra */
inline void ecraLinha(const std::string& s="") {
    std::cout << boxL(s);
}
inline void ecraVazia() {
    std::cout << boxL("");
}
inline void ecraSep(const std::string& label="") {
    if(label.empty()) {
        std::cout << boxL(" "+std::string(UI_IW-2,'-'));
    } else {
        std::string ln=" \xe2\x94\x80\xe2\x94\x80 "+label+" ";
        int rem=UI_IW-(int)ln.size()-1;
        if(rem<0)rem=0;
        std::cout << boxL(ln+std::string(rem,'-')+" ");
    }
}

/* Mensagens de estado */
inline void ecraOk(const std::string& m)  { std::cout<<boxL(" [OK]  "+m); }
inline void ecraErr(const std::string& m) { std::cout<<boxL(" [!!]  "+m); }
inline void ecraInfo(const std::string& m){ std::cout<<boxL(" [i]   "+m); }

/* Legado */
inline void msgOk(const std::string& m)  { ecraOk(m);  }
inline void msgErr(const std::string& m) { ecraErr(m); }
inline void msgInfo(const std::string& m){ ecraInfo(m);}
inline void titulo(const std::string& t)    { ecraVazia(); ecraSep(t); ecraVazia(); }
inline void subtitulo(const std::string& t) { ecraVazia(); ecraSep(t); }
inline void linha(char='-',int=0) { ecraSep(); }
inline void sep(char='-',int=0)   { ecraSep(); }
inline void secTitle(const std::string& t) { ecraVazia(); ecraSep(t); ecraVazia(); }

/* ── Input ──────────────────────────────────────────────────── */

/* Imprimir prompt, ler input na mesma linha */
static inline std::string inputLinha(const std::string& prompt) {
    /* Imprimir a linha do box com o prompt */
    std::string ln=" > "+prompt;
    int vl=visLen(ln);
    int pad=UI_IW-vl; if(pad<0)pad=0;
    /* Imprimir sem newline, cursor fica a seguir ao prompt */
    std::cout << BX_V << ln;
    std::cout.flush();
    std::string s;
    std::getline(std::cin,s);
    while(!s.empty()&&s.back()=='\r') s.pop_back();
    while(!s.empty()&&(s.front()==' '||s.front()=='\t')) s.erase(s.begin());
    while(!s.empty()&&(s.back()==' '||s.back()=='\t')) s.pop_back();
    /* Reescrever a linha completa com a resposta */
    std::cout<<"\033[1A\033[2K";
    std::cout<<boxL(ln+s);
    return s;
}

inline std::string lerString(const std::string& prompt) {
    return inputLinha(prompt);
}

inline int lerInteiro(const std::string& prompt, int mn=0, int mx=999999) {
    while(true) {
        std::string s=inputLinha(prompt);
        try {
            int v=std::stoi(s);
            if(v>=mn&&v<=mx) return v;
            ecraErr("Valor entre "+std::to_string(mn)+" e "+std::to_string(mx));
        } catch(...) {
            if(!s.empty()) ecraErr("Numero invalido");
        }
    }
}

inline double lerDouble(const std::string& prompt) {
    while(true) {
        std::string s=inputLinha(prompt);
        std::replace(s.begin(),s.end(),',','.');
        try {
            double v=std::stod(s);
            if(v>=0) return v;
            ecraErr("Valor positivo");
        } catch(...) {
            if(!s.empty()) ecraErr("Numero invalido");
        }
    }
}

inline bool lerSimNao(const std::string& prompt) {
    while(true) {
        std::string s=lerString(prompt+" [s/n]: ");
        if(s=="s"||s=="S"||s=="sim") return true;
        if(s=="n"||s=="N"||s=="nao") return false;
        ecraErr("Responda 's' ou 'n'");
    }
}

inline void pausar(const std::string& msg="Prima Enter para continuar...") {
    fecharEcra();
    std::string ln=" "+msg;
    int pad=UI_IW-(int)ln.size(); if(pad<0)pad=0;
    std::cout << BX_V << ln << std::string(pad,' ');
    std::cout.flush();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    std::cout << BX_V << "\n" << botLine();
}

/* ── Permissoes ─────────────────────────────────────────────── */
inline bool temPermissao(const std::string& rm) {
    auto n=[](const std::string& r){
        if(r=="admin")    return 4;
        if(r=="gerente")  return 3;
        if(r=="vendedor") return 2;
        if(r=="tecnico")  return 1;
        return 0;
    };
    return n(g_sessao.role)>=n(rm);
}
inline void erroPermissao() {
    ecraErr("Sem permissao para esta acao.");
    pausar();
}

/* ── Numeros de documento ───────────────────────────────────── */
inline std::string gerarNumeroFatura(int seq) {
    std::ostringstream o; o<<"FAT-"<<std::setw(6)<<std::setfill('0')<<seq; return o.str();
}
inline std::string gerarNumeroOrcamento(int seq) {
    std::ostringstream o; o<<"ORC-"<<std::setw(6)<<std::setfill('0')<<seq; return o.str();
}
inline std::string gerarNumeroReparacao(int seq) {
    std::ostringstream o; o<<"REP-"<<std::setw(6)<<std::setfill('0')<<seq; return o.str();
}
