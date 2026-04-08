#pragma once
/*
 * common.h - UI TUI estilo SSN, cross-platform (Windows + Linux + macOS)
 */

/* ── Stdlib ─────────────────────────────────────────────────── */
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <limits>
#include <vector>
#include <map>
#include <fstream>

/* ── Platform ───────────────────────────────────────────────── */
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <conio.h>
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
  #define PLATFORM_SLEEP(s) Sleep((DWORD)((s)*1000))
#else
  #include <sys/stat.h>
  #include <unistd.h>
  #define MKDIR(p) mkdir((p),0755)
  #define PLATFORM_SLEEP(s) sleep(s)
#endif

/* ── Paths ──────────────────────────────────────────────────── */
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

/* ── UI: largura do ecra ────────────────────────────────────── */
#define UI_W 80

/* ── Sessao ─────────────────────────────────────────────────── */
struct Sessao {
    std::string username, nome, role, loja_id, loja_nome;
    bool autenticado;
    Sessao() : autenticado(false) {}
};
extern Sessao       g_sessao;
extern std::string  g_breadcrumb;
extern std::string  g_dica;
extern std::string  g_loja_nome;

/* ── Inicializar consola ────────────────────────────────────── */
inline void uiInitConsole() {
#ifdef _WIN32
    /* UTF-8 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    /* ANSI no Windows 10+ */
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode))
            SetConsoleMode(h, mode | 0x0004); /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
    }
    /* Nota: resize de janela omitido por compatibilidade */
#endif
}

/* ── Codigos ANSI ───────────────────────────────────────────── */
#define A_RST  "\033[0m"
#define A_BOLD "\033[1m"
#define A_DIM  "\033[2m"
#define A_REV  "\033[7m"
#define A_W    "\033[97m"
#define A_Y    "\033[93m"
#define A_G    "\033[92m"
#define A_C    "\033[96m"
#define A_R    "\033[91m"

/* ── Utilitarios de string ──────────────────────────────────── */
inline std::string fw(const std::string& s, int w, char fill=' ') {
    std::string r = s;
    if ((int)r.size() > w) r = r.substr(0, w);
    while ((int)r.size() < w) r += fill;
    return r;
}
inline std::string fwR(const std::string& s, int w) {
    std::string r = s;
    if ((int)r.size() > w) r = r.substr(0, w);
    while ((int)r.size() < w) r = " " + r;
    return r;
}
inline std::string fwL(const std::string& s, int w) { return fwR(s,w); }
inline std::string rep(char c, int n) { return std::string(n, c); }

/* ── Datas ──────────────────────────────────────────────────── */
inline std::string dataAtual() {
    std::time_t t = std::time(0); char b[32];
    std::strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",std::localtime(&t));
    return b;
}
inline std::string dataApenasData() {
    std::time_t t = std::time(0); char b[16];
    std::strftime(b,sizeof(b),"%Y-%m-%d",std::localtime(&t));
    return b;
}
inline std::string dataFormatada() {
    std::time_t t = std::time(0); char b[16];
    std::strftime(b,sizeof(b),"%d-%m-%Y",std::localtime(&t));
    return b;
}
inline std::string horaAtual() {
    std::time_t t = std::time(0); char b[10];
    std::strftime(b,sizeof(b),"%H:%M:%S",std::localtime(&t));
    return b;
}
inline std::string adicionarDias(const std::string& ds, int dias) {
    struct tm tm2; memset(&tm2,0,sizeof(tm2));
    int y=0,mo=0,d=0; sscanf(ds.c_str(),"%d-%d-%d",&y,&mo,&d);
    tm2.tm_year=y-1900; tm2.tm_mon=mo-1; tm2.tm_mday=d;
    std::time_t t = std::mktime(&tm2); t += (std::time_t)dias*86400;
    char buf[16]; std::strftime(buf,sizeof(buf),"%Y-%m-%d",std::localtime(&t));
    return buf;
}

/* ── Limpar ecra ────────────────────────────────────────────── */
inline void cls() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
    std::cout.flush();
}

/* ── Linha horizontal ───────────────────────────────────────── */
inline void hline(char c='-', int n=UI_W) {
    std::cout << std::string(n,c) << "\n";
}

/* ── Cabecalho estilo SSN ───────────────────────────────────── */
inline void desenharCabecalho() {
    std::string loja = g_sessao.loja_nome.empty() ? "TECHFIX" : g_sessao.loja_nome;
    std::string user = g_sessao.username;
    std::transform(user.begin(),user.end(),user.begin(),::toupper);
    std::string nome = g_sessao.nome.empty() ? user : g_sessao.nome;
    std::transform(nome.begin(),nome.end(),nome.begin(),::toupper);

    /* Linha 1 */
    std::string l1e = fw(loja,21);
    std::string l1m = "VENDEDOR ("+user+") -"+fw(nome,25);
    std::string l1d = "DATA :";
    std::cout << A_BOLD << A_W << fw(l1e,21) << fw(l1m,46) << fw(l1d,13) << A_RST << "\n";

    /* Linha 2 */
    std::string lid  = g_sessao.loja_id.empty() ? "0001" : g_sessao.loja_id.substr(0,12);
    std::string l2e  = fw(lid+" - "+user, 21);
    std::string l2m  = fw("MOVIMENTO (.) -", 46);
    std::string data = dataFormatada();
    std::cout << A_W << l2e << l2m << A_REV << " " << fw(data,10) << " " << A_RST << "\n";

    hline('=');
}

/* ── iniciarEcra / fecharEcra ───────────────────────────────── */
inline void iniciarEcra(const std::string& /*titulo*/="",
                        const std::string& breadcrumb="",
                        const std::string& dica="") {
    if (!breadcrumb.empty()) g_breadcrumb = breadcrumb;
    if (!dica.empty())       g_dica = dica;
    cls();
    desenharCabecalho();
}

inline void fecharEcra() {
    std::cout << "\n";
    hline('-');
    std::string esq = " " + g_breadcrumb;
    std::string dir;
    if (!g_dica.empty()) dir = "[ " + g_dica + " ] ";
    int sp = UI_W - (int)esq.size() - (int)dir.size();
    if (sp < 1) sp = 1;
    std::cout << A_DIM << esq << std::string(sp,' ') << dir << A_RST << "\n";
}

/* ── Mensagens ──────────────────────────────────────────────── */
inline void ecraLinha(const std::string& s="") { std::cout << s << "\n"; }
inline void ecraVazia()                         { std::cout << "\n"; }
inline void ecraSep(const std::string& lbl="") {
    if (lbl.empty()) { hline('-',UI_W-2); }
    else { std::cout << A_BOLD << "-- " << lbl << " "; hline('-',UI_W-4-(int)lbl.size()); std::cout << A_RST; }
}
inline void ecraOk(const std::string& m)   { std::cout<<A_G<<A_BOLD<<" [OK]  "<<A_RST<<A_W<<m<<A_RST<<"\n"; }
inline void ecraErr(const std::string& m)  { std::cout<<A_R<<A_BOLD<<" [!!]  "<<A_RST<<A_W<<m<<A_RST<<"\n"; }
inline void ecraInfo(const std::string& m) { std::cout<<A_C<<"  >> "<<A_RST<<A_W<<m<<A_RST<<"\n"; }

inline void msgOk(const std::string& m)  { ecraOk(m);  }
inline void msgErr(const std::string& m) { ecraErr(m); }
inline void msgInfo(const std::string& m){ ecraInfo(m);}

/* Compatibilidade */
inline void titulo(const std::string& t)    { std::cout<<"\n"; ecraSep(t); }
inline void subtitulo(const std::string& t) { std::cout<<"\n"; ecraSep(t); }
inline void linha(char='-',int=0)           { ecraSep(); }
inline void sep(char='-',int=0)             { ecraSep(); }
inline void secTitle(const std::string& t)  { std::cout<<"\n"; ecraSep(t); }
inline void boxL(const std::string& s)      { std::cout<<s<<"\n"; }

/* ── Input ──────────────────────────────────────────────────── */
inline std::string lerString(const std::string& prompt) {
    std::cout << A_W << "  " << prompt << A_RST;
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    while (!s.empty() && (s.back()=='\r'||s.back()=='\n')) s.pop_back();
    while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
    while (!s.empty() && (s.back()==' '||s.back()=='\t'))  s.pop_back();
    return s;
}

inline int lerInteiro(const std::string& prompt, int mn=0, int mx=999999) {
    while (true) {
        std::string s = lerString(prompt);
        if (s.empty()) continue;
        try {
            int v = std::stoi(s);
            if (v >= mn && v <= mx) return v;
            ecraErr("Valor entre "+std::to_string(mn)+" e "+std::to_string(mx));
        } catch (...) { ecraErr("Numero invalido"); }
    }
}

inline double lerDouble(const std::string& prompt) {
    while (true) {
        std::string s = lerString(prompt);
        std::replace(s.begin(),s.end(),',','.');
        if (s.empty()) continue;
        try {
            double v = std::stod(s);
            if (v >= 0) return v;
            ecraErr("Valor positivo");
        } catch (...) { ecraErr("Numero invalido"); }
    }
}

inline bool lerSimNao(const std::string& prompt) {
    while (true) {
        std::string s = lerString(prompt+" [s/n]: ");
        if (s=="s"||s=="S"||s=="sim"||s=="SIM") return true;
        if (s=="n"||s=="N"||s=="nao"||s=="NAO") return false;
        ecraErr("Responda s ou n");
    }
}

/* ── Ler opcao de menu (letra/numero) ───────────────────────── */
inline char lerOpcaoMenu(const std::string& prompt="Opcao: ") {
    std::cout << "\n" << A_BOLD << A_Y << "  " << prompt << A_RST;
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    while (!s.empty() && (s.back()=='\r'||s.back()=='\n')) s.pop_back();
    if (s.empty()) return '\0';
    return (char)std::toupper((unsigned char)s[0]);
}

/* ── Ler password sem eco ───────────────────────────────────── */
inline std::string lerPassword(const std::string& prompt) {
    std::cout << A_W << "  " << prompt << A_RST;
    std::cout.flush();
    std::string pw;
#ifdef _WIN32
    int c;
    while ((c=_getch()) != '\r' && c != '\n' && c != EOF) {
        if (c=='\b' || c==127) {
            if (!pw.empty()) { pw.pop_back(); std::cout<<"\b \b"; std::cout.flush(); }
        } else if (c >= 32) {
            pw += (char)c;
            std::cout << '*'; std::cout.flush();
        }
    }
    std::cout << "\n";
#else
    std::cout << "\033[8m"; /* esconder texto */
    std::getline(std::cin, pw);
    std::cout << "\033[28m\n"; /* mostrar texto */
#endif
    while (!pw.empty() && (pw.back()=='\r'||pw.back()=='\n')) pw.pop_back();
    return pw;
}

inline void pausar(const std::string& msg="Prima ENTER para continuar...") {
    std::cout << "\n" << A_DIM << "  " << msg << A_RST;
    std::cout.flush();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

/* ── Menu estilo SSN ────────────────────────────────────────── */
struct MenuItem { std::string tecla; std::string label; };

inline void menuItem(const std::string& tecla, const std::string& label) {
    std::cout << A_BOLD << A_Y << tecla << A_RST << A_W << "-" << label << A_RST;
}

inline void menuDuasColunas(const std::vector<MenuItem>& esq,
                             const std::vector<MenuItem>& dir,
                             int col_esq=40) {
    size_t n = std::max(esq.size(), dir.size());
    for (size_t i=0; i<n; ++i) {
        if (i<esq.size()) {
            int vis = (int)esq[i].tecla.size()+1+(int)esq[i].label.size();
            menuItem(esq[i].tecla, esq[i].label);
            if (vis<col_esq) std::cout << std::string(col_esq-vis,' ');
        } else {
            std::cout << std::string(col_esq,' ');
        }
        if (i<dir.size()) menuItem(dir[i].tecla, dir[i].label);
        std::cout << "\n";
    }
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
    return n(g_sessao.role) >= n(rm);
}
inline void erroPermissao() { ecraErr("Sem permissao para esta acao."); pausar(); }

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
