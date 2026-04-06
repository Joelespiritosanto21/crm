#pragma once
/*
 * common.h - Interface TUI estilo terminal classico anos 80/90
 *
 * Inspirado no sistema SSN da Radio Popular:
 * - Fundo preto, texto branco/amarelo
 * - Cabecalho com loja, vendedor, data
 * - Menus com tecla de letra destacada
 * - Sem bordas decorativas - texto puro
 * - Navegacao por tecla unica (sem Enter para menus)
 */

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <limits>
#include <vector>
#include <map>
#include <cstdio>
#include <cstring>

/* ── Largura do ecra ──────────────────────────────────────── */
#define UI_W  80

/* ── Paths ─────────────────────────────────────────────────── */
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

/* ── Sessao global ─────────────────────────────────────────── */
struct Sessao {
    std::string username;
    std::string nome;
    std::string role;
    std::string loja_id;
    std::string loja_nome;
    bool autenticado;
    Sessao() : autenticado(false) {}
};
extern Sessao       g_sessao;
extern std::string  g_breadcrumb;
extern std::string  g_dica;
extern std::string  g_loja_nome;

/* ── Inicializar consola Windows ───────────────────────────── */
#ifdef _WIN32
  #include <windows.h>
  #include <conio.h>
  inline void uiInitConsole() {
      SetConsoleOutputCP(65001);
      SetConsoleCP(65001);
      /* Modo ANSI no Windows 10+ */
      HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
      DWORD mode = 0;
      GetConsoleMode(h, &mode);
      SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
#else
  inline void uiInitConsole() {}
#endif

/* ── Codigos ANSI ──────────────────────────────────────────── */
#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_DIM        "\033[2m"
#define ANSI_REVERSE    "\033[7m"
#define ANSI_WHITE      "\033[97m"
#define ANSI_YELLOW     "\033[93m"
#define ANSI_GREEN      "\033[92m"
#define ANSI_CYAN       "\033[96m"
#define ANSI_RED        "\033[91m"
#define ANSI_BLACK      "\033[30m"
#define ANSI_BG_BLACK   "\033[40m"
#define ANSI_BG_WHITE   "\033[107m"
#define ANSI_BG_BLUE    "\033[44m"
#define ANSI_BG_CYAN    "\033[46m"

/* ── Utilitarios de string ─────────────────────────────────── */
inline std::string fw(const std::string& s, int w, char fill=' ') {
    std::string r=s;
    if((int)r.size()>w) r=r.substr(0,w);
    while((int)r.size()<w) r+=fill;
    return r;
}
inline std::string fwR(const std::string& s, int w) {
    std::string r=s;
    if((int)r.size()>w) r=r.substr(0,w);
    while((int)r.size()<w) r=" "+r;
    return r;
}
inline std::string fwL(const std::string& s, int w) { return fwR(s,w); }
inline std::string rep(char c, int n) { return std::string(n,c); }

/* ── Datas ─────────────────────────────────────────────────── */
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
inline std::string dataFormatada() {
    /* formato DD-MM-YYYY como no SSN */
    std::time_t t=std::time(0); char b[16];
    std::strftime(b,sizeof(b),"%d-%m-%Y",std::localtime(&t));
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

/* ── Limpar ecra ───────────────────────────────────────────── */
inline void cls() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
    std::cout.flush();
}

/* ── Linha horizontal de tracos ────────────────────────────── */
inline void hline(char c='-', int n=UI_W) {
    std::cout << rep(c,n) << "\n";
}

/* ================================================================
 * CABECALHO ESTILO SSN
 *
 *  TECHFIX - ESTARREJA        VENDEDOR (ADM) -NOME COMPLETO    DATA :
 *  0001 - rep001              MOVIMENTO (.) -                  12-04-2026
 * ================================================================ */
inline void desenharCabecalho() {
    std::string loja   = fw(g_sessao.loja_nome.empty() ? "TECHFIX" : g_sessao.loja_nome, 20);
    std::string user   = g_sessao.username;
    std::transform(user.begin(),user.end(),user.begin(),::toupper);
    std::string nome   = g_sessao.nome.empty() ? g_sessao.username : g_sessao.nome;
    std::transform(nome.begin(),nome.end(),nome.begin(),::toupper);
    std::string data   = dataFormatada();

    /* Linha 1 */
    std::string l1_esq = loja;
    std::string l1_mid = "VENDEDOR (" + user + ") -" + fw(nome,25);
    std::string l1_dir = "DATA :";
    /* total=80: esq=21 mid=46 dir=13 */
    std::cout << ANSI_BOLD << ANSI_WHITE
              << fw(l1_esq,21) << fw(l1_mid,46) << fw(l1_dir,13)
              << ANSI_RESET << "\n";

    /* Linha 2 */
    std::string num_loja = g_sessao.loja_id.empty() ? "0001" : g_sessao.loja_id.substr(0,4);
    std::string num_user = g_sessao.username.empty() ? "---" : g_sessao.username;
    std::string l2_esq  = num_loja + " - " + num_user;
    std::string l2_mid  = "MOVIMENTO (.) -";
    std::string l2_dir  = std::string(ANSI_REVERSE) + " " + fw(data,10) + " " + ANSI_RESET;

    std::cout << ANSI_WHITE
              << fw(l2_esq,21) << fw(l2_mid,46);
    /* Data invertida (fundo branco, texto preto) */
    std::cout << ANSI_REVERSE << " " << fw(data,10) << " " << ANSI_RESET
              << "\n";

    /* Linha separadora */
    hline('-');
}

/* ================================================================
 * CABECALHO DE LISTAGEM (ARTIGO / QUANTID / DESCRICAO / ...)
 * Personalizavel conforme o ecra
 * ================================================================ */
inline void cabecalhoTabela(const std::vector<std::pair<std::string,int>>& cols) {
    std::string linha1, linha2;
    for(auto& c : cols) {
        linha1 += fw(c.first, c.second);
        linha2 += rep('-', c.second-1) + " ";
    }
    std::cout << ANSI_BOLD << ANSI_WHITE << linha1 << ANSI_RESET << "\n";
    std::cout << linha2 << "\n";
}

/* ================================================================
 * ITEM DE MENU ESTILO SSN
 * Tecla destacada (bold/cor) seguida do texto
 *
 * Exemplo: "V-Venda a Publico"  → V em destaque
 * ================================================================ */
inline void menuItem(const std::string& tecla, const std::string& label, int col_w=0) {
    std::string item;
    item += ANSI_BOLD;
    item += ANSI_YELLOW;
    item += tecla;
    item += ANSI_RESET;
    item += ANSI_WHITE;
    item += "-";
    item += label;
    item += ANSI_RESET;
    if(col_w>0) {
        /* padding para alinhamento em colunas */
        int visible = (int)tecla.size() + 1 + (int)label.size();
        if(visible<col_w) item += rep(' ', col_w-visible);
    }
    std::cout << item;
}

/* Imprimir menu em 2 colunas como no SSN */
struct MenuItem { std::string tecla; std::string label; };

inline void menuDuasColunas(const std::vector<MenuItem>& esq,
                             const std::vector<MenuItem>& dir,
                             int col_esq=38) {
    size_t n = std::max(esq.size(), dir.size());
    for(size_t i=0; i<n; ++i) {
        /* coluna esquerda */
        if(i<esq.size()) {
            int vis = (int)esq[i].tecla.size()+1+(int)esq[i].label.size();
            menuItem(esq[i].tecla, esq[i].label);
            if(vis<col_esq) std::cout << rep(' ',col_esq-vis);
        } else {
            std::cout << rep(' ',col_esq);
        }
        /* coluna direita */
        if(i<dir.size()) {
            menuItem(dir[i].tecla, dir[i].label);
        }
        std::cout << "\n";
    }
}

/* Menu numa so coluna */
inline void menuUmaColuna(const std::vector<MenuItem>& items) {
    for(auto& m : items) {
        menuItem(m.tecla, m.label);
        std::cout << "\n";
    }
}

/* ================================================================
 * INICIAR / FECHAR ECRA
 * ================================================================ */
inline void iniciarEcra(const std::string& /*titulo*/="",
                        const std::string& breadcrumb="",
                        const std::string& dica="") {
    if(!breadcrumb.empty()) g_breadcrumb=breadcrumb;
    if(!dica.empty())       g_dica=dica;
    cls();
    std::cout << ANSI_BG_BLACK;
    desenharCabecalho();
}

inline void fecharEcra() {
    /* Linha de estado no fundo - como o "SUB-TOTAL" do SSN */
    std::cout << "\n";
    hline('-');
    if(!g_dica.empty()) {
        std::cout << ANSI_YELLOW << g_dica << ANSI_RESET;
        int pad = UI_W - (int)g_dica.size() - (int)g_breadcrumb.size() - 4;
        if(pad>0) std::cout << rep(' ',pad);
    }
    std::string rb = "[ " + g_breadcrumb + " ]";
    std::cout << ANSI_DIM << rb << ANSI_RESET << "\n";
}

/* ── Mensagens ─────────────────────────────────────────────── */
inline void msgOk(const std::string& m) {
    std::cout << "\n" << ANSI_GREEN << ANSI_BOLD << "  OK: " << ANSI_RESET << ANSI_WHITE << m << ANSI_RESET << "\n";
}
inline void msgErr(const std::string& m) {
    std::cout << "\n" << ANSI_RED << ANSI_BOLD << "  !! " << ANSI_RESET << ANSI_WHITE << m << ANSI_RESET << "\n";
}
inline void msgInfo(const std::string& m) {
    std::cout << "\n" << ANSI_CYAN << "  >> " << ANSI_RESET << ANSI_WHITE << m << ANSI_RESET << "\n";
}
inline void ecraOk(const std::string& m)  { msgOk(m);  }
inline void ecraErr(const std::string& m) { msgErr(m); }
inline void ecraInfo(const std::string& m){ msgInfo(m);}

/* ── Legado (compatibilidade) ──────────────────────────────── */
inline void ecraLinha(const std::string& s="") { std::cout << s << "\n"; }
inline void ecraVazia()                         { std::cout << "\n"; }
inline void ecraSep(const std::string& lbl="") {
    if(lbl.empty()) { hline('-'); }
    else { std::cout << ANSI_BOLD << "-- " << lbl << " "; hline('-', UI_W-4-(int)lbl.size()); std::cout << ANSI_RESET; }
}
inline void titulo(const std::string& t)    { std::cout << "\n"; ecraSep(t); }
inline void subtitulo(const std::string& t) { std::cout << "\n"; ecraSep(t); }
inline void linha(char c='-',int n=UI_W)    { hline(c,n); }
inline void sep(char c='-',int n=UI_W)      { hline(c,n); }
inline void secTitle(const std::string& t)  { std::cout << "\n"; ecraSep(t); }
inline void boxL(const std::string& s)      { std::cout << s << "\n"; }

/* ================================================================
 * INPUT — ler tecla unica (para menus) ou linha completa
 * ================================================================ */

/* Ler um unico caracter sem Enter (getch) */
inline char lerTecla() {
#ifdef _WIN32
    return (char)_getch();
#else
    /* Simples: ler linha e pegar primeiro char */
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    if(s.empty()) return '\n';
    return s[0];
#endif
}

inline std::string lerString(const std::string& prompt) {
    std::cout << ANSI_WHITE << "  " << prompt << ANSI_RESET;
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    while(!s.empty()&&s.back()=='\r') s.pop_back();
    while(!s.empty()&&(s.front()==' '||s.front()=='\t')) s.erase(s.begin());
    while(!s.empty()&&(s.back()==' '||s.back()=='\t')) s.pop_back();
    return s;
}

inline int lerInteiro(const std::string& prompt, int mn=0, int mx=999999) {
    while(true) {
        std::cout << ANSI_WHITE << "  " << prompt << ANSI_RESET;
        std::cout.flush();
        std::string s;
        std::getline(std::cin, s);
        while(!s.empty()&&s.back()=='\r') s.pop_back();
        try {
            int v=std::stoi(s);
            if(v>=mn&&v<=mx) return v;
            msgErr("Valor entre "+std::to_string(mn)+" e "+std::to_string(mx));
        } catch(...) {
            if(!s.empty()) msgErr("Numero invalido");
        }
    }
}

inline double lerDouble(const std::string& prompt) {
    while(true) {
        std::cout << ANSI_WHITE << "  " << prompt << ANSI_RESET;
        std::cout.flush();
        std::string s;
        std::getline(std::cin, s);
        while(!s.empty()&&s.back()=='\r') s.pop_back();
        std::replace(s.begin(),s.end(),',','.');
        try {
            double v=std::stod(s);
            if(v>=0) return v;
            msgErr("Valor positivo");
        } catch(...) {
            if(!s.empty()) msgErr("Numero invalido");
        }
    }
}

inline bool lerSimNao(const std::string& prompt) {
    while(true) {
        std::cout << ANSI_WHITE << "  " << prompt << " (S/N): " << ANSI_RESET;
        std::cout.flush();
        std::string s;
        std::getline(std::cin, s);
        while(!s.empty()&&s.back()=='\r') s.pop_back();
        if(s=="s"||s=="S"||s=="sim"||s=="SIM") return true;
        if(s=="n"||s=="N"||s=="nao"||s=="NAO") return false;
        msgErr("Responda S ou N");
    }
}

/* Ler opcao de menu por letra/numero */
inline char lerOpcaoMenu(const std::string& prompt="Opcao: ") {
    std::cout << "\n" << ANSI_BOLD << ANSI_YELLOW << "  " << prompt << ANSI_RESET;
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    while(!s.empty()&&s.back()=='\r') s.pop_back();
    if(s.empty()) return '\0';
    return (char)std::toupper(s[0]);
}

inline void pausar(const std::string& msg="Prima ENTER para continuar...") {
    std::cout << "\n" << ANSI_DIM << "  " << msg << ANSI_RESET;
    std::cout.flush();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
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
    msgErr("Sem permissao para esta acao.");
    pausar();
}

/* ── Numeros de documento ────────────────────────────────────── */
inline std::string gerarNumeroFatura(int seq) {
    std::ostringstream o; o<<"FAT-"<<std::setw(6)<<std::setfill('0')<<seq; return o.str();
}
inline std::string gerarNumeroOrcamento(int seq) {
    std::ostringstream o; o<<"ORC-"<<std::setw(6)<<std::setfill('0')<<seq; return o.str();
}
inline std::string gerarNumeroReparacao(int seq) {
    std::ostringstream o; o<<"REP-"<<std::setw(6)<<std::setfill('0')<<seq; return o.str();
}
