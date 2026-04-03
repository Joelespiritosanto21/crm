#pragma once
/*
 * server.h - Servidor HTTP simples na porta 2021
 * Interface web para o sistema de gestão
 * Sem dependências externas - usa apenas sockets padrão Windows/Unix
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <string.h>
    #define SOCKET_ERROR_VALUE -1
    #define CLOSE_SOCKET(s) close(s)
    typedef int SOCKET;
    #define INVALID_SOCKET -1
#endif

/* ============================================================
 * Estrutura de resposta HTTP
 * ============================================================ */
struct RespostaHTTP {
    int codigo;
    std::string tipo_conteudo;
    std::string corpo;
    
    RespostaHTTP(int _codigo = 200, const std::string& _tipo = "text/html", 
                 const std::string& _corpo = "")
        : codigo(_codigo), tipo_conteudo(_tipo), corpo(_corpo) {}
    
    std::string serializar() const {
        std::string resposta;
        resposta += "HTTP/1.1 " + std::to_string(codigo) + " OK\r\n";
        resposta += "Content-Type: " + tipo_conteudo + "; charset=utf-8\r\n";
        resposta += "Content-Length: " + std::to_string(corpo.length()) + "\r\n";
        resposta += "Access-Control-Allow-Origin: *\r\n";
        resposta += "Connection: close\r\n";
        resposta += "\r\n";
        resposta += corpo;
        return resposta;
    }
};

/* ============================================================
 * Parser HTTP minimalista
 * ============================================================ */
struct RequisicaoHTTP {
    std::string metodo;
    std::string caminho;
    std::string versao;
    std::map<std::string, std::string> cabecalhos;
    std::string corpo;
    
    static RequisicaoHTTP parsear(const std::string& dados) {
        RequisicaoHTTP req;
        std::istringstream iss(dados);
        
        // Linha 1: METHOD PATH HTTP/VERSION
        iss >> req.metodo >> req.caminho >> req.versao;
        
        // Cabecalhos
        std::string linha;
        std::getline(iss, linha); // linha em branco inicial
        while (std::getline(iss, linha) && !linha.empty() && linha != "\r") {
            if (linha.back() == '\r') linha.pop_back();
            size_t pos = linha.find(": ");
            if (pos != std::string::npos) {
                std::string chave = linha.substr(0, pos);
                std::string valor = linha.substr(pos + 2);
                req.cabecalhos[chave] = valor;
            }
        }
        
        // Corpo
        std::string resto;
        while (std::getline(iss, resto)) {
            req.corpo += resto + "\n";
        }
        
        return req;
    }
};

/* ============================================================
 * Servidor HTTP
 * ============================================================ */
class ServidorHTTP {
private:
    SOCKET socket_servidor = INVALID_SOCKET;
    int porta = 2021;
    bool rodando = false;
    std::function<RespostaHTTP(const RequisicaoHTTP&)> manipulador_rotas;
    
public:
    ServidorHTTP(int _porta = 2021) : porta(_porta) {}
    
    ~ServidorHTTP() {
        parar();
    }
    
    /* Inicializar Winsock (Windows) */
    static void inicializarWinsock() {
#ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            std::cerr << "Erro ao inicializar Winsock\n";
        }
#endif
    }
    
    /* Limpar Winsock (Windows) */
    static void limparWinsock() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    /* Registar manipulador de rotas */
    void setManipulador(std::function<RespostaHTTP(const RequisicaoHTTP&)> fn) {
        manipulador_rotas = fn;
    }
    
    /* Iniciar servidor */
    bool iniciar() {
        inicializarWinsock();
        
        // Criar socket
        socket_servidor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_servidor == INVALID_SOCKET) {
            std::cerr << "Erro ao criar socket\n";
            return false;
        }
        
        // Bind
        struct sockaddr_in endereco;
#ifdef _WIN32
        memset(&endereco, 0, sizeof(endereco));
#else
        bzero(&endereco, sizeof(endereco));
#endif
        endereco.sin_family = AF_INET;
        endereco.sin_addr.s_addr = htonl(INADDR_ANY);
        endereco.sin_port = htons(porta);
        
        if (bind(socket_servidor, (struct sockaddr*)&endereco, sizeof(endereco)) == SOCKET_ERROR_VALUE) {
            std::cerr << "Erro ao fazer bind na porta " << porta << "\n";
            CLOSE_SOCKET(socket_servidor);
            return false;
        }
        
        // Listen
        if (listen(socket_servidor, 5) == SOCKET_ERROR_VALUE) {
            std::cerr << "Erro ao iniciar listen\n";
            CLOSE_SOCKET(socket_servidor);
            return false;
        }
        
        rodando = true;
        std::cout << "\n[SERVIDOR] Ouvindo na porta " << porta << "...\n";
        return true;
    }
    
    /* Parar servidor */
    void parar() {
        rodando = false;
        if (socket_servidor != INVALID_SOCKET) {
            CLOSE_SOCKET(socket_servidor);
            socket_servidor = INVALID_SOCKET;
        }
        limparWinsock();
    }
    
    /* Processar uma requisição */
    void processar() {
        if (!rodando) return;
        
        struct sockaddr_in cliente_addr;
        socklen_t cliente_addr_len = sizeof(cliente_addr);
        
        // Accept (não-bloqueante seria ideal, mas deixamos simples)
        SOCKET cliente_socket = accept(socket_servidor, (struct sockaddr*)&cliente_addr, &cliente_addr_len);
        
        if (cliente_socket == INVALID_SOCKET) {
            return; // Erro ou timeout
        }
        
        // Receber dados
        char buffer[4096] = {0};
        int bytes_recebidos = recv(cliente_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_recebidos > 0) {
            buffer[bytes_recebidos] = '\0';
            std::string dados_recebidos(buffer);
            
            // Parsear requisição
            RequisicaoHTTP req = RequisicaoHTTP::parsear(dados_recebidos);
            
            // Chamar manipulador
            RespostaHTTP resp;
            if (manipulador_rotas) {
                resp = manipulador_rotas(req);
            } else {
                resp = RespostaHTTP(404, "text/plain", "404 - Não encontrado");
            }
            
            // Enviar resposta
            std::string resposta_str = resp.serializar();
            send(cliente_socket, resposta_str.c_str(), (int)resposta_str.length(), 0);
        }
        
        // Fechar conexão
        CLOSE_SOCKET(cliente_socket);
    }
    
    /* Loop de aceitação de conexões (bloqueante) */
    void rodar_bloqueante() {
        while (rodando) {
            processar();
        }
    }
    
    bool estaRodando() const {
        return rodando;
    }
    
    SOCKET obterSocket() const {
        return socket_servidor;
    }
};

/* ============================================================
 * Utilitários HTTP
 * ============================================================ */
inline std::string escaparHTML(const std::string& texto) {
    std::string resultado;
    for (char c : texto) {
        switch (c) {
            case '&': resultado += "&amp;"; break;
            case '<': resultado += "&lt;"; break;
            case '>': resultado += "&gt;"; break;
            case '"': resultado += "&quot;"; break;
            case '\'': resultado += "&#39;"; break;
            default: resultado += c;
        }
    }
    return resultado;
}

inline std::string descodificarURL(const std::string& url) {
    std::string resultado;
    for (size_t i = 0; i < url.length(); ++i) {
        if (url[i] == '%' && i + 2 < url.length()) {
            int hex = 0;
            sscanf(url.substr(i + 1, 2).c_str(), "%x", &hex);
            resultado += (char)hex;
            i += 2;
        } else if (url[i] == '+') {
            resultado += ' ';
        } else {
            resultado += url[i];
        }
    }
    return resultado;
}

/* ============================================================
 * Função de inicialização do servidor (definida em server.cpp)
 * ============================================================ */
bool iniciarServidorWeb();
void pararServidorWeb();
