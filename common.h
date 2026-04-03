#pragma once
/*
 * common.h - Tipos e utilitários partilhados
 */

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <limits>
#include <vector>

/* ============================================================
 * Constantes de paths
 * ============================================================ */
#define DATA_DIR   "data/"
#define DOCS_DIR   "docs/"

#define FILE_UTILIZADORES  DATA_DIR "utilizadores.json"
#define FILE_CLIENTES      DATA_DIR "clientes.json"
#define FILE_PRODUTOS      DATA_DIR "produtos.json"
#define FILE_VENDAS        DATA_DIR "vendas.json"
#define FILE_ORCAMENTOS    DATA_DIR "orcamentos.json"
#define FILE_REPARACOES    DATA_DIR "reparacoes.json"
#define FILE_GARANTIAS     DATA_DIR "garantias.json"
#define FILE_LOJAS         DATA_DIR "lojas.json"
#define FILE_LOGS          DATA_DIR "logs.json"

/* ============================================================
 * Sessão global
 * ============================================================ */
struct Sessao {
    std::string username;
    std::string role;      // admin, gerente, vendedor, tecnico
    std::string loja_id;
    bool autenticado{false};
};

extern Sessao g_sessao;

/* ============================================================
 * Formatação de datas
 * ============================================================ */
inline std::string dataAtual() {
    auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

inline std::string dataApenasData() {
    auto t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return buf;
}

inline std::string adicionarDias(const std::string& data_str, int dias) {
    // Parsear data YYYY-MM-DD
    struct tm tm{};
    std::istringstream ss(data_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    time_t t = std::mktime(&tm);
    t += (time_t)dias * 86400;
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return buf;
}

/* ============================================================
 * UI helpers
 * ============================================================ */
inline void limparEcra() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

inline void pausar() {
    std::cout << "\n[Enter para continuar...]";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

inline void linha(char c = '-', int n = 60) {
    std::cout << std::string(n, c) << "\n";
}

inline void titulo(const std::string& t) {
    linha('=');
    std::cout << "  " << t << "\n";
    linha('=');
}

inline void subtitulo(const std::string& t) {
    std::cout << "\n";
    linha('-');
    std::cout << "  " << t << "\n";
    linha('-');
}

inline std::string lerString(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    // Trim
    while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='\r')) s.pop_back();
    return s;
}

inline int lerInteiro(const std::string& prompt, int min=0, int max=999999) {
    while (true) {
        std::cout << prompt;
        std::string s;
        std::getline(std::cin, s);
        try {
            int v = std::stoi(s);
            if (v >= min && v <= max) return v;
            std::cout << "  Valor entre " << min << " e " << max << ".\n";
        } catch (...) {
            std::cout << "  Número inválido.\n";
        }
    }
}

inline double lerDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string s;
        std::getline(std::cin, s);
        // Aceitar vírgula ou ponto como separador decimal
        std::replace(s.begin(), s.end(), ',', '.');
        try {
            double v = std::stod(s);
            if (v >= 0) return v;
            std::cout << "  Valor deve ser positivo.\n";
        } catch (...) {
            std::cout << "  Número inválido.\n";
        }
    }
}

inline bool lerSimNao(const std::string& prompt) {
    while (true) {
        std::string s = lerString(prompt + " (s/n): ");
        if (s=="s"||s=="S"||s=="sim"||s=="Sim") return true;
        if (s=="n"||s=="N"||s=="nao"||s=="não"||s=="Não") return false;
        std::cout << "  Responda 's' ou 'n'.\n";
    }
}

/* ============================================================
 * Permissões
 * ============================================================ */
inline bool temPermissao(const std::string& role_min) {
    // Hierarquia: admin > gerente > vendedor > tecnico
    auto nivel = [](const std::string& r) {
        if (r=="admin")   return 4;
        if (r=="gerente") return 3;
        if (r=="vendedor")return 2;
        if (r=="tecnico") return 1;
        return 0;
    };
    return nivel(g_sessao.role) >= nivel(role_min);
}

inline void erroPermissao() {
    std::cout << "\n  [!] Sem permissão para esta ação.\n";
}

/* ============================================================
 * Geração de números de documento
 * ============================================================ */
inline std::string gerarNumeroFatura(int seq) {
    std::ostringstream o;
    o << "FAT-" << std::setw(6) << std::setfill('0') << seq;
    return o.str();
}
inline std::string gerarNumeroOrcamento(int seq) {
    std::ostringstream o;
    o << "ORC-" << std::setw(6) << std::setfill('0') << seq;
    return o.str();
}
inline std::string gerarNumeroReparacao(int seq) {
    std::ostringstream o;
    o << "REP-" << std::setw(6) << std::setfill('0') << seq;
    return o.str();
}

/* ============================================================
 * Instância global de UI (definida em main.cpp)
 * ============================================================ */
#include "ui.h"
extern InterfaceUI g_ui;
