#pragma once
/*
 * ui.h - Sistema de UI melhorado com limpeza automática, tamanho fixo
 * e suporte a Windows/Linux sem cores
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

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif

/* ============================================================
 * Constantes de tamanho
 * ============================================================ */
#define LARGURA_JANELA 100
#define ALTURA_JANELA  30

/* ============================================================
 * Sistema de UI com limpeza automática
 * ============================================================ */
class InterfaceUI {
private:
    bool primeiro_run = true;
    std::string buffer_saida;
    
public:
    InterfaceUI() = default;
    
    /* Inicializar interface (tamanho, limpeza) */
    void inicializar() {
        limparEcraCompleto();
        definirTamanhoJanela(LARGURA_JANELA, ALTURA_JANELA);
        primeiro_run = false;
    }
    
    /* Limpar ecrã completamente */
    void limparEcraCompleto() {
#ifdef _WIN32
        (void)system("cls");
#else
        (void)system("clear");
#endif
    }
    
    /* Definir tamanho da janela (funciona em Linux e Windows moderno) */
    void definirTamanhoJanela(int largura, int altura) {
#ifdef _WIN32
        // Windows
        std::string cmd = "mode con: cols=" + std::to_string(largura) 
                          + " lines=" + std::to_string(altura);
        system(cmd.c_str());
#else
        // Linux/macOS - enviar escape sequence
        // printf("\033]8;;;;;?\007\033]8;;;;;?\007");
        // Este é mais uma sugestão visual, não é obrigatório
#endif
    }
    
    /* Mostrar menu com limpeza automática */
    void mostrarMenu(const std::string& titulo_menu, const std::vector<std::string>& opcoes) {
        limparEcraCompleto();
        mostraTimeslot();
        linha('=', LARGURA_JANELA);
        std::cout << "  " << titulo_menu << "\n";
        linha('=', LARGURA_JANELA);
        std::cout << "\n";
        
        for (size_t i = 0; i < opcoes.size(); ++i) {
            std::cout << "  [" << (i + 1) << "] " << opcoes[i] << "\n";
        }
        
        std::cout << "\n  [0] Sair\n\n";
        std::cout << "  Escolha uma opção: ";
    }
    
    /* Mostrar submenu */
    void mostrarSubmenu(const std::string& titulo, const std::vector<std::string>& opcoes) {
        limparEcraCompleto();
        mostraTimeslot();
        subtitulo(titulo);
        
        for (size_t i = 0; i < opcoes.size(); ++i) {
            std::cout << "  [" << (i + 1) << "] " << opcoes[i] << "\n";
        }
        
        std::cout << "\n  [0] Voltar atrás\n\n";
        std::cout << "  Escolha: ";
    }
    
    /* Limpar após escolha e mostrar resultado */
    void mostrarResultado(const std::string& titulo, const std::string& conteudo) {
        limparEcraCompleto();
        mostraTimeslot();
        titulo_grande(titulo);
        std::cout << conteudo << "\n";
    }
    
    /* Painel de informação com hora/data */
    void mostraTimeslot() {
        auto t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        
        linha('=', LARGURA_JANELA);
        std::cout << "  [ " << buf << " ]\n";
        linha('=', LARGURA_JANELA);
        std::cout << "\n";
    }
    
    /* Títulos formatados */
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
    
    /* Linha separadora */
    void linha(char c = '-', int n = LARGURA_JANELA) {
        std::cout << std::string(n, c) << "\n";
    }
    
    /* Pausar com mensagem customizada */
    void pausar(const std::string& msg = "[ENTER para continuar...]") {
        std::cout << "\n  " << msg;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
    }
    
    /* Caixa decorativa */
    void mostrarCaixa(const std::string& titulo, const std::string& conteudo) {
        linha('+', LARGURA_JANELA);
        std::cout << "| " << titulo << "\n";
        linha('+', LARGURA_JANELA);
        std::cout << conteudo << "\n";
        linha('+', LARGURA_JANELA);
    }
    
    /* Mostrar tabela simples */
    void mostrarTabela(const std::vector<std::vector<std::string>>& dados) {
        for (const auto& linha : dados) {
            std::cout << "  ";
            for (const auto& celula : linha) {
                std::cout << celula << "  ";
            }
            std::cout << "\n";
        }
    }
    
    /* Esperar com delay (para efeito visual) */
    void delay(int ms) {
        // Implementação simplificada: usar sleep padrão
#ifdef _WIN32
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }
};

/* Instância global de UI */
extern InterfaceUI g_ui;
