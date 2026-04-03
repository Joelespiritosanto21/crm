#pragma once
/*
 * ui.h - Sistema de UI profissional com formatação avançada
 * Suporte Windows/Linux, tabelas, menus, caixas decorativas
 */

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <limits>
#include <cstdio>
#include <ctime>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif

/* ============================================================
 * Constantes de tamanho e cores
 * ============================================================ */
#define LARGURA_JANELA 110
#define ALTURA_JANELA  35

// ANSI Color codes para terminal (funciona na maioria dos sistemas)
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define ITALIC  "\033[3m"
#define UNDERLINE "\033[4m"

#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

#define BG_BLACK   "\033[40m"
#define BG_RED     "\033[41m"
#define BG_GREEN   "\033[42m"
#define BG_YELLOW  "\033[43m"
#define BG_BLUE    "\033[44m"

/* ============================================================
 * Sistema de UI profissional com limpeza automática
 * ============================================================ */
class InterfaceUI {
private:
    bool primeiro_run = true;
    int tamanho_tabela_default = 110;
    
public:
    InterfaceUI() = default;
    
    /* ========== INICIALIZAÇÃO E LIMPEZA ========== */
    
    void inicializar() {
        limparEcraCompleto();
        definirTamanhoJanela(LARGURA_JANELA, ALTURA_JANELA);
        primeiro_run = false;
    }
    
    void limparEcraCompleto() {
#ifdef _WIN32
        (void)system("cls");
#else
        (void)system("clear");
#endif
    }
    
    void definirTamanhoJanela(int largura, int altura) {
#ifdef _WIN32
        std::string cmd = "mode con: cols=" + std::to_string(largura) 
                          + " lines=" + std::to_string(altura);
        system(cmd.c_str());
#endif
    }
    
    /* ========== CABEÇALHO E RODAPÉ ========== */
    
    void mostraTimeslot() {
        auto t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", std::localtime(&t));
        
        caixaHorizontal('═', LARGURA_JANELA);
        std::cout << "  " << CYAN << BOLD << "║" << RESET << "  " 
                  << BOLD << buf << RESET 
                  << std::string(LARGURA_JANELA - 30 - buf[0], ' ')
                  << CYAN << BOLD << "║" << RESET << "\n";
        caixaHorizontal('═', LARGURA_JANELA);
        std::cout << "\n";
    }
    
    void mostraRodape() {
        std::cout << "\n";
        caixaHorizontal('═', LARGURA_JANELA);
        std::cout << "  " << CYAN << BOLD << "║" << RESET << "  "
                  << DIM << "Sistema de Gestão - Versão 2.0" << RESET
                  << std::string(70, ' ')
                  << CYAN << BOLD << "║" << RESET << "\n";
        caixaHorizontal('═', LARGURA_JANELA);
    }
    
    /* ========== TÍTULOS E CABEÇALHOS ========== */
    
    void tituloprincipal(const std::string& titulo) {
        limparEcraCompleto();
        mostraTimeslot();
        
        caixaDourada();
        int padding = (LARGURA_JANELA - titulo.length() - 4) / 2;
        std::cout << "  " << YELLOW << BOLD << "▓▓ " << titulo << " ▓▓" << RESET << "\n";
        caixaDourada();
        std::cout << "\n";
    }
    
    void secao(const std::string& titulo) {
        std::cout << "\n";
        caixaHorizontal('─', LARGURA_JANELA);
        std::cout << "  " << BLUE << BOLD << "► " << titulo << RESET << "\n";
        caixaHorizontal('─', LARGURA_JANELA);
        std::cout << "\n";
    }
    
    void subsecao(const std::string& titulo) {
        std::cout << "\n  " << MAGENTA << "└─ " << titulo << RESET << "\n";
        std::cout << "  " << MAGENTA << "   " << std::string(titulo.length(), '─') << RESET << "\n\n";
    }
    
    /* ========== MENUS ========== */
    
    void mostrarMenu(const std::string& titulo_menu, const std::vector<std::string>& opcoes) {
        limparEcraCompleto();
        mostraTimeslot();
        
        titulosimples(titulo_menu);
        
        std::cout << "\n";
        for (size_t i = 0; i < opcoes.size(); ++i) {
            std::cout << "     " << CYAN << "[" << i + 1 << "]" << RESET << "  " 
                      << opcoes[i] << "\n";
        }
        
        std::cout << "\n     " << RED << "[0]" << RESET << "  Sair\n\n";
        std::cout << "  ➜ Opção: " << BOLD;
        std::cout.flush();
    }
    
    void mostrarSubmenu(const std::string& titulo, const std::vector<std::string>& opcoes) {
        limparEcraCompleto();
        mostraTimeslot();
        titulosimples(titulo);
        
        std::cout << "\n";
        for (size_t i = 0; i < opcoes.size(); ++i) {
            std::cout << "     " << GREEN << "[" << i + 1 << "]" << RESET << "  " 
                      << opcoes[i] << "\n";
        }
        
        std::cout << "\n     " << RED << "[0]" << RESET << "  Voltar\n\n";
        std::cout << "  ➜ Opção: " << BOLD;
        std::cout.flush();
    }
    
    void titulosimples(const std::string& titulo) {
        caixaHorizontal('▬', LARGURA_JANELA);
        std::cout << "  " << BLUE << BOLD << titulo << RESET << "\n";
        caixaHorizontal('▬', LARGURA_JANELA);
    }
    
    /* ========== TABELAS PROFISSIONAIS ========== */
    
    void mostrarTabela(const std::vector<std::string>& cabecalhos, 
                       const std::vector<std::vector<std::string>>& dados) {
        size_t num_cols = cabecalhos.size();
        std::vector<size_t> larguras_col(num_cols, 12);
        
        // Calcular larguras baseado no conteúdo
        for (const auto& linha : dados) {
            for (size_t i = 0; i < linha.size() && i < num_cols; ++i) {
                larguras_col[i] = std::max(larguras_col[i], linha[i].length() + 2);
            }
        }
        
        for (size_t i = 0; i < num_cols; ++i) {
            larguras_col[i] = std::max(larguras_col[i], cabecalhos[i].length() + 2);
        }
        
        // Cabeçalho
        std::cout << "\n  ";
        for (size_t i = 0; i < num_cols; ++i) {
            if (i == 0) std::cout << "┌";
            else std::cout << "┬";
            std::cout << std::string(larguras_col[i], '─');
        }
        std::cout << "┐\n";
        
        std::cout << "  │ " << CYAN << BOLD;
        for (size_t i = 0; i < num_cols; ++i) {
            std::cout << std::left << std::setw(larguras_col[i]) << cabecalhos[i] << RESET;
            std::cout << CYAN << BOLD << "│ " << RESET;
        }
        std::cout << "\n";
        
        // Separador
        std::cout << "  ";
        for (size_t i = 0; i < num_cols; ++i) {
            if (i == 0) std::cout << "├";
            else std::cout << "┼";
            std::cout << std::string(larguras_col[i], '═');
        }
        std::cout << "┤\n";
        
        // Dados
        bool alt = false;
        for (const auto& linha : dados) {
            std::cout << "  │ ";
            if (alt) std::cout << GREEN;
            for (size_t i = 0; i < num_cols && i < linha.size(); ++i) {
                std::cout << std::left << std::setw(larguras_col[i]) << linha[i];
                std::cout << "│ ";
            }
            if (alt) std::cout << RESET;
            std::cout << "\n";
            alt = !alt;
        }
        
        // Rodapé
        std::cout << "  ";
        for (size_t i = 0; i < num_cols; ++i) {
            if (i == 0) std::cout << "└";
            else std::cout << "┴";
            std::cout << std::string(larguras_col[i], '─');
        }
        std::cout << "┘\n\n";
    }
    
    /* ========== MENSAGENS DE STATUS ========== */
    
    void sucesso(const std::string& mensagem) {
        std::cout << "\n  " << GREEN << BOLD << "✓ SUCESSO" << RESET << GREEN 
                  << " — " << mensagem << RESET << "\n\n";
    }
    
    void erro(const std::string& mensagem) {
        std::cout << "\n  " << RED << BOLD << "✗ ERRO" << RESET << RED 
                  << " — " << mensagem << RESET << "\n\n";
    }
    
    void aviso(const std::string& mensagem) {
        std::cout << "\n  " << YELLOW << BOLD << "⚠ AVISO" << RESET << YELLOW 
                  << " — " << mensagem << RESET << "\n\n";
    }
    
    void info(const std::string& mensagem) {
        std::cout << "\n  " << CYAN << "ℹ INFO" << RESET << CYAN 
                  << " — " << mensagem << RESET << "\n\n";
    }
    
    /* ========== CAIXAS DECORATIVAS ========== */
    
    void caixaDestaque(const std::string& titulo, const std::string& conteudo) {
        std::cout << "\n  ";
        caixaHorizontal('▲', LARGURA_JANELA - 4);
        std::cout << "  " << YELLOW << BOLD << "║ " << titulo << " ║" << RESET << "\n";
        std::cout << "  ";
        caixaHorizontal('▼', LARGURA_JANELA - 4);
        std::cout << "  " << conteudo << "\n";
        std::cout << "  ";
        caixaHorizontal('▼', LARGURA_JANELA - 4);
        std::cout << "\n";
    }
    
    void caixaSimples(const std::string& conteudo) {
        std::cout << "\n  " << BLUE << "┌" << std::string(LARGURA_JANELA - 6, '─') << "┐" << RESET << "\n";
        std::cout << "  " << BLUE << "│" << RESET << "  " << conteudo 
                  << std::string(LARGURA_JANELA - 8 - conteudo.length(), ' ')
                  << "  " << BLUE << "│" << RESET << "\n";
        std::cout << "  " << BLUE << "└" << std::string(LARGURA_JANELA - 6, '─') << "┘" << RESET << "\n\n";
    }
    
    /* ========== ENTRADA DE DADOS ========== */
    
    std::string lerTexto(const std::string& prompt = "") {
        if (!prompt.empty()) {
            std::cout << "  " << BOLD << prompt << RESET << " ";
        }
        std::string entrada;
        std::getline(std::cin, entrada);
        return entrada;
    }
    
    int lerInteiro(const std::string& prompt = "") {
        std::string entrada;
        while (true) {
            entrada = lerTexto(prompt);
            try {
                return std::stoi(entrada);
            } catch (...) {
                erro("Valor inválido. Digite um número inteiro.");
            }
        }
    }
    
    double lerDouble(const std::string& prompt = "") {
        std::string entrada;
        while (true) {
            entrada = lerTexto(prompt);
            try {
                return std::stod(entrada);
            } catch (...) {
                erro("Valor inválido. Digite um número decimal.");
            }
        }
    }
    
    /* ========== UTILITÁRIOS ========== */
    
    void pausar(const std::string& msg = "[ENTER para continuar...]") {
        std::cout << "\n  " << DIM << msg << RESET;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    
    void delay(int ms = 1000) {
#ifdef _WIN32
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }
    
private:
    void caixaHorizontal(char c, int tamanho) {
        std::cout << "  " << CYAN;
        for (int i = 0; i < tamanho; ++i) std::cout << c;
        std::cout << RESET << "\n";
    }
    
    void caixaDourada() {
        std::cout << "  ";
        for (int i = 0; i < LARGURA_JANELA - 4; ++i) std::cout << YELLOW << "▓" << RESET;
        std::cout << "\n";
    }
    
    void mostrarResultado(const std::string& titulo, const std::string& conteudo) {
        tituloprincipal(titulo);
        std::cout << conteudo << "\n";
    }
    
    void linha(char c = '-', int n = LARGURA_JANELA) {
        std::cout << std::string(n, c) << "\n";
    }
    
    void titulo_grande(const std::string& t, int largura = LARGURA_JANELA) {
        linha('=', largura);
        int padding = (largura - t.length()) / 2;
        std::cout << std::string(padding, ' ') << t << "\n";
        linha('=', largura);
        std::cout << "\n";
    }
    
    void subtitulo(const std::string& t, int largura = LARGURA_JANELA) {
        std::cout << "\n";
        linha('-', largura);
        std::cout << "  " << t << "\n";
        linha('-', largura);
        std::cout << "\n";
    }
    
    void mostrarTabela(const std::vector<std::vector<std::string>>& dados) {
        for (const auto& linha : dados) {
            std::cout << "  ";
            for (const auto& celula : linha) {
                std::cout << celula << "  ";
            }
            std::cout << "\n";
        }
    }
    
    void mostrarCaixa(const std::string& titulo, const std::string& conteudo) {
        linha('+', LARGURA_JANELA);
        std::cout << "| " << titulo << "\n";
        linha('+', LARGURA_JANELA);
        std::cout << conteudo << "\n";
        linha('+', LARGURA_JANELA);
    }
};

/* Instância global de UI */
extern InterfaceUI g_ui;
