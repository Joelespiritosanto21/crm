/*
 * server.cpp - Servidor HTTP simples na porta 2021
 */

#include "server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <chrono>

/* ============================================================
 * Página HTML para terminal web
 * ============================================================ */
static std::string obterPaginaHTML() {
    return R"EOF(<!DOCTYPE html>
<html lang="pt-PT">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CRM - Sistema de Gestão</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Courier New', monospace;
            background-color: #000;
            color: #fff;
            padding: 20px;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
            border: 2px solid #fff;
            padding: 20px;
        }
        h1 {
            text-align: center;
            border-bottom: 2px solid #fff;
            padding-bottom: 10px;
            margin-bottom: 20px;
            letter-spacing: 2px;
        }
        .status {
            background-color: #001a00;
            border: 1px solid #00ff00;
            padding: 15px;
            margin: 20px 0;
            border-radius: 3px;
        }
        .info {
            font-size: 14px;
            line-height: 1.8;
            margin: 10px 0;
        }
        .info strong {
            color: #00ff00;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Sistema de Gestão - Interface Web</h1>
        
        <div class="status">
            <div class="info"><strong>Status:</strong> ✓ SERVIDOR ONLINE</div>
            <div class="info"><strong>Porta:</strong> 2021</div>
            <div class="info"><strong>Interface:</strong> 0.0.0.0 (Todas as interfaces)</div>
            <div class="info"><strong>Hora:</strong> <span id="hora"></span></div>
        </div>
        
        <div style="margin-top: 20px; padding: 15px; border: 1px solid #666;">
            <h2 style="margin-bottom: 10px;">Endpoints Disponíveis</h2>
            <div class="info">
                GET  / &rarr; Esta página<br>
                GET  /api/status &rarr; Status JSON<br>
                POST /api/cmd &rarr; Executar comando
            </div>
        </div>
        
        <div style="margin-top: 20px; padding: 15px; border: 1px solid #666;">
            <h2 style="margin-bottom: 10px;">Testes Rápidos</h2>
            <div class="info">
curl http://localhost:2021/<br>
curl http://localhost:2021/api/status<br>
curl -X POST http://localhost:2021/api/cmd -H "Content-Type: application/json" -d '{"comando":"help"}'
            </div>
        </div>
    </div>
    
    <script>
        function atualizarHora() {
            document.getElementById('hora').innerText = new Date().toLocaleString('pt-PT');
        }
        atualizarHora();
        setInterval(atualizarHora, 1000);
    </script>
</body>
</html>)EOF";
}

/* ============================================================
 * Manipulador de rotas HTTP
 * ============================================================ */
static RespostaHTTP manipularRota(const RequisicaoHTTP& req) {
    // GET /
    if (req.metodo == "GET" && req.caminho == "/") {
        return RespostaHTTP(200, "text/html", obterPaginaHTML());
    }
    
    // GET /index.html
    if (req.metodo == "GET" && req.caminho == "/index.html") {
        return RespostaHTTP(200, "text/html", obterPaginaHTML());
    }
    
    // GET /api/status
    if (req.metodo == "GET" && req.caminho == "/api/status") {
        return RespostaHTTP(200, "application/json", 
            "{\"status\":\"online\",\"porta\":2021,\"interface\":\"0.0.0.0\"}");
    }
    
    // POST /api/cmd
    if (req.metodo == "POST" && req.caminho == "/api/cmd") {
        return RespostaHTTP(200, "application/json", 
            "{\"resultado\":\"Comando recebido\"}");
    }
    
    // 404
    return RespostaHTTP(404, "text/plain", "404 - Não encontrado");
}

/* ============================================================
 * Variáveis globais do servidor
 * ============================================================ */
static std::unique_ptr<ServidorHTTP> g_servidor;
static std::unique_ptr<std::thread> g_thread_servidor;
static volatile bool servidor_parado = false;

/* ============================================================
 * Função para rodar o servidor em background
 * ============================================================ */
static void rodarServidorBackground() {
    std::cout << "[SERVIDOR] Thread de servidor iniciada\n";
    
    while (g_servidor && g_servidor->estaRodando() && !servidor_parado) {
        g_servidor->processar();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[SERVIDOR] Thread de servidor encerrada\n";
}

/* ============================================================
 * Iniciar servidor (chamado de main.cpp)
 * ============================================================ */
bool iniciarServidorWeb() {
    try {
        // Criar instância do servidor na porta 2021
        g_servidor = std::make_unique<ServidorHTTP>(2021);
        
        // Registar manipulador de rotas
        g_servidor->setManipulador(manipularRota);
        
        // Iniciar
        if (!g_servidor->iniciar()) {
            std::cerr << "[ERRO] Falha ao iniciar servidor HTTP\n";
            return false;
        }
        
        servidor_parado = false;
        
        // Rodar em thread separada
        g_thread_servidor = std::make_unique<std::thread>(rodarServidorBackground);
        g_thread_servidor->detach();
        
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║           SERVIDOR WEB INICIADO COM SUCESSO                ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════╣\n";
        std::cout << "║ Porta: 2021                                                ║\n";
        std::cout << "║ Interface: 0.0.0.0 (Todos os IPs)                         ║\n";
        std::cout << "║                                                            ║\n";
        std::cout << "║ Acesso:                                                    ║\n";
        std::cout << "║  • Localhost:  http://localhost:2021                      ║\n";
        std::cout << "║  • Remoto:     http://<seu-ip>:2021                       ║\n";
        std::cout << "║                                                            ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERRO] Exceção ao iniciar servidor: " << e.what() << "\n";
        return false;
    }
}

/* ============================================================
 * Parar servidor (para limpeza no exit)
 * ============================================================ */
void pararServidorWeb() {
    servidor_parado = true;
    if (g_servidor) {
        g_servidor->parar();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
