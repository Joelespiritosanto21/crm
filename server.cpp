/*
 * server.cpp - Implementação do servidor HTTP e interface web
 */

#include "server.h"
#include <sstream>
#include <iostream>
#include <cstdlib>

/* ============================================================
 * Interface HTML para terminal web
 * ============================================================ */
static std::string obterPaginaHTML() {
    return R"EOF(
<!DOCTYPE html>
<html lang="pt-PT">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sistema de Gestão - Terminal Web</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Courier New', monospace;
            background-color: #000;
            color: #fff;
            line-height: 1.5;
            padding: 0;
        }
        
        .container {
            display: flex;
            height: 100vh;
        }
        
        .terminal {
            width: 100%;
            background-color: #000;
            border: 2px solid #fff;
            padding: 20px;
            overflow-y: auto;
            display: flex;
            flex-direction: column;
            font-size: 14px;
        }
        
        .titulo-terminal {
            border: 2px solid #fff;
            padding: 10px;
            margin-bottom: 20px;
            text-align: center;
            font-weight: bold;
            letter-spacing: 2px;
        }
        
        .output {
            flex: 1;
            white-space: pre-wrap;
            word-wrap: break-word;
            overflow-y: auto;
            margin-bottom: 20px;
            border: 1px solid #666;
            padding: 10px;
        }
        
        .input-area {
            display: flex;
            gap: 10px;
        }
        
        .input-area input {
            flex: 1;
            background-color: #000;
            border: 1px solid #fff;
            color: #fff;
            padding: 10px;
            font-family: 'Courier New', monospace;
            font-size: 14px;
        }
        
        .input-area input:focus {
            outline: none;
            border: 2px solid #fff;
        }
        
        .input-area button {
            background-color: #fff;
            border: 1px solid #fff;
            color: #000;
            padding: 10px 20px;
            font-weight: bold;
            cursor: pointer;
            font-family: 'Courier New', monospace;
            font-size: 14px;
        }
        
        .input-area button:hover {
            background-color: #ccc;
        }
        
        .info {
            margin-top: 10px;
            font-size: 12px;
            color: #888;
        }
        
        .espera {
            display: none;
            text-align: center;
            padding: 10px;
        }
        
        .espera.ativo {
            display: block;
            color: #fff;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="terminal">
            <div class="titulo-terminal">
                SISTEMA DE GESTÃO - INTERFACE WEB
            </div>
            <div class="output" id="output">
[SISTEMA] Bem-vindo ao Sistema de Gestão via Web
[SISTEMA] Digite um comando ou pressione Enter para menu

            </div>
            <div class="espera" id="espera">
                [ Processando... ]
            </div>
            <div class="input-area">
                <input type="text" id="cmdInput" placeholder="Digite comando ou selecione opção..." />
                <button onclick="enviarComando()">ENVIAR</button>
            </div>
            <div class="info">
                Porta: 2021 | Compatibilidade: Windows/Linux/macOS
            </div>
        </div>
    </div>
    
    <script>
        const output = document.getElementById("output");
        const cmdInput = document.getElementById("cmdInput");
        const espera = document.getElementById("espera");
        
        cmdInput.addEventListener("keypress", function(e) {
            if (e.key === "Enter") {
                enviarComando();
            }
        });
        
        function enviarComando() {
            const comando = cmdInput.value.trim();
            if (!comando) return;
            
            output.innerText += "\n> " + comando + "\n";
            cmdInput.value = "";
            
            espera.classList.add("ativo");
            
            fetch("/api/cmd", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ comando: comando })
            })
            .then(r => r.json())
            .then(data => {
                espera.classList.remove("ativo");
                output.innerText += data.resultado + "\n";
                output.scrollTop = output.scrollHeight;
            })
            .catch(err => {
                espera.classList.remove("ativo");
                output.innerText += "ERRO: " + err.message + "\n";
            });
        }
        
        window.addEventListener("load", function() {
            cmdInput.focus();
        });
        
        setInterval(function() {
            fetch("/api/status")
                .then(r => r.text())
                .then(data => {
                })
                .catch(err => console.log("Status check error"));
        }, 1000);
    </script>
</body>
</html>
    )EOF";
}

/* ============================================================
 * Variável global para comunicação
 * ============================================================ */
static std::string ultimo_comando = "";
static std::string ultima_saida = "";
static bool tem_comando_novo = false;

/* ============================================================
 * Manipulador de rotas HTTP
 * ============================================================ */
RespostaHTTP manipularRota(const RequisicaoHTTP& req) {
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
            "{\"status\":\"ok\",\"tempo\":\"" + std::string(__TIME__) + "\"}");
    }
    
    // POST /api/cmd
    if (req.metodo == "POST" && req.caminho.substr(0, 8) == "/api/cmd") {
        // Parsear JSON (minimalista)
        size_t pos = req.corpo.find("\"comando\":");
        if (pos != std::string::npos) {
            size_t inicio = req.corpo.find("\"", pos + 10) + 1;
            size_t fim = req.corpo.find("\"", inicio);
            std::string comando = req.corpo.substr(inicio, fim - inicio);
            
            // Armazenar comando para processar na thread principal
            ultimo_comando = comando;
            tem_comando_novo = true;
            
            // Por enquanto, retornar resposta com echo do comando
            std::string resposta = 
                "{\"resultado\":\"Comando recebido: " + escaparHTML(comando) + 
                "\\n[Sistema] Execute na interface CLI para processar.\\n\"}";
            
            return RespostaHTTP(200, "application/json", resposta);
        }
        
        return RespostaHTTP(400, "application/json", 
            "{\"erro\":\"Comando inválido\"}");
    }
    
    // 404
    return RespostaHTTP(404, "text/plain", "404 - Rota não encontrada");
}

/* ============================================================
 * Iniciar servidor (chamado de main.cpp)
 * Nota: Versão simplificada sem threads no MVP
 * ============================================================ */
bool iniciarServidorWeb() {
    // Servidor web será implementado na próxima versão
    // Por enquanto, retorna sucesso para manter compatibilidade
    std::cout << "[INFO] Servidor web na porta 2021 será implementado em versão futura\n";
    return true;
}
