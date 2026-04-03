/*
 * documentos.cpp - Geração de documentos
 */

#include "documentos.h"
#include "clientes.h"
#include "produtos.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <sys/stat.h>

/* ================================================================
 * Inicializar pasta docs
 * ================================================================ */
void docInicializar() {
    MKDIR(DOCS_DIR);
}

/* ================================================================
 * Linha separadora HTML/TXT
 * ================================================================ */
static std::string linhaHTML(int n=60) {
    return "<hr style='border:1px solid #ccc;margin:8px 0'>";
}
static std::string linhaTXT(char c='-', int n=60) {
    return std::string(n, c) + "\n";
}

/* ================================================================
 * Cabeçalho HTML comum
 * ================================================================ */
static std::string htmlCabecalho(const std::string& titulo) {
    return R"(<!DOCTYPE html><html lang="pt"><head><meta charset="UTF-8">
<title>)" + titulo + R"(</title>
<style>
  body{font-family:Arial,sans-serif;max-width:700px;margin:40px auto;padding:20px;color:#222}
  h1{color:#1a237e;font-size:22px;margin-bottom:4px}
  h2{color:#283593;font-size:16px}
  table{width:100%;border-collapse:collapse;margin:12px 0}
  th{background:#1a237e;color:#fff;padding:8px;text-align:left;font-size:13px}
  td{padding:7px 8px;border-bottom:1px solid #e0e0e0;font-size:13px}
  tr:nth-child(even) td{background:#f5f5f5}
  .total{font-size:18px;font-weight:bold;color:#1a237e;text-align:right;margin-top:16px}
  .badge{display:inline-block;padding:3px 10px;border-radius:12px;font-size:12px;font-weight:bold}
  .rodape{margin-top:30px;font-size:11px;color:#888;text-align:center}
  .info{background:#f0f4ff;padding:12px;border-radius:6px;margin:10px 0;font-size:13px}
</style>
</head><body>
<h1>)" + titulo + R"(</h1>)" "\n";
}

/* ================================================================
 * Gerar Fatura HTML
 * ================================================================ */
std::string docGerarFatura(const JsonValue& venda) {
    std::string cliente_nome = clienteObterNome(venda["cliente_id"].asString());
    std::string numero       = venda["numero"].asString();
    std::string data         = venda["data"].asString();
    double total             = venda["total"].asDouble();

    std::string filepath = std::string(DOCS_DIR) + numero + ".html";
    std::ofstream f(filepath);
    if (!f.is_open()) return "";

    f << htmlCabecalho("Fatura " + numero);
    f << "<div class='info'>";
    f << "<b>Nº:</b> " << numero << "<br>";
    f << "<b>Data:</b> " << data << "<br>";
    f << "<b>Cliente:</b> " << cliente_nome << "<br>";
    f << "<b>Emitido por:</b> " << g_sessao.username << "<br>";
    f << "</div>\n";

    f << "<table><thead><tr><th>Produto/Serviço</th><th>Qtd</th>"
         "<th>Preço Unit.</th><th>Subtotal</th></tr></thead><tbody>\n";

    if (venda["itens"].isArray()) {
        for (auto& item : venda["itens"].arr) {
            double pu       = item["preco_unitario"].asDouble();
            int    qty      = (int)item["quantidade"].asInt();
            double subtotal = pu * qty;
            f << "<tr><td>" << item["nome"].asString() << "</td>"
              << "<td>" << qty << "</td>"
              << "<td>" << std::fixed << std::setprecision(2) << pu << " €</td>"
              << "<td>" << std::fixed << std::setprecision(2) << subtotal << " €</td></tr>\n";
        }
    }

    f << "</tbody></table>\n";
    f << "<div class='total'>Total: " << std::fixed << std::setprecision(2) << total << " €</div>\n";
    f << "<div class='rodape'>Obrigado pela sua preferência | " << dataAtual() << "</div>\n";
    f << "</body></html>\n";
    f.close();
    return filepath;
}

/* ================================================================
 * Gerar Orçamento HTML
 * ================================================================ */
std::string docGerarOrcamento(const JsonValue& orc) {
    std::string cliente_nome = clienteObterNome(orc["cliente_id"].asString());
    std::string numero       = orc["numero"].asString();
    std::string data         = orc["data"].asString();
    std::string estado       = orc["estado"].asString();
    double total             = orc["total"].asDouble();

    std::string filepath = std::string(DOCS_DIR) + numero + ".html";
    std::ofstream f(filepath);
    if (!f.is_open()) return "";

    f << htmlCabecalho("Orçamento " + numero);
    f << "<div class='info'>";
    f << "<b>Nº:</b> " << numero << "<br>";
    f << "<b>Data:</b> " << data << "<br>";
    f << "<b>Cliente:</b> " << cliente_nome << "<br>";
    f << "<b>Estado:</b> " << estado << "<br>";
    f << "<b>Válido por:</b> 30 dias<br>";
    f << "</div>\n";

    f << "<table><thead><tr><th>Item</th><th>Qtd</th>"
         "<th>Preço Unit.</th><th>Subtotal</th></tr></thead><tbody>\n";

    if (orc["itens"].isArray()) {
        for (auto& item : orc["itens"].arr) {
            double pu       = item["preco_unitario"].asDouble();
            int    qty      = (int)item["quantidade"].asInt();
            double subtotal = pu * qty;
            f << "<tr><td>" << item["nome"].asString() << "</td>"
              << "<td>" << qty << "</td>"
              << "<td>" << std::fixed << std::setprecision(2) << pu << " €</td>"
              << "<td>" << std::fixed << std::setprecision(2) << subtotal << " €</td></tr>\n";
        }
    }

    f << "</tbody></table>\n";
    f << "<div class='total'>Total Estimado: " << std::fixed << std::setprecision(2) << total << " €</div>\n";
    f << "<p style='font-size:12px;color:#666'>Este orçamento não constitui fatura.</p>\n";
    f << "<div class='rodape'>" << dataAtual() << "</div>\n";
    f << "</body></html>\n";
    f.close();
    return filepath;
}

/* ================================================================
 * Gerar Certificado de Garantia HTML
 * ================================================================ */
std::string docGerarGarantia(const JsonValue& garantia, const std::string& cliente_nome) {
    std::string id        = garantia["id"].asString();
    std::string item      = garantia["item"].asString();
    std::string data_ini  = garantia["data_inicio"].asString();
    std::string data_fim  = garantia["data_fim"].asString();
    int         duracao   = (int)garantia["duracao_dias"].asInt();

    std::string filepath = std::string(DOCS_DIR) + "GAR_" + id + ".html";
    std::ofstream f(filepath);
    if (!f.is_open()) return "";

    f << htmlCabecalho("Certificado de Garantia");
    f << "<div class='info'>";
    f << "<b>ID:</b> " << id << "<br>";
    f << "<b>Cliente:</b> " << cliente_nome << "<br>";
    f << "<b>Produto/Serviço:</b> " << item << "<br>";
    f << "<b>Data de início:</b> " << data_ini << "<br>";
    f << "<b>Data de fim:</b> " << data_fim << "<br>";
    f << "<b>Duração:</b> " << duracao << " dias<br>";
    if (!garantia["referencia"].asString().empty())
        f << "<b>Referência:</b> " << garantia["referencia"].asString() << "<br>";
    f << "</div>\n";
    f << "<p>Este certificado garante o produto/serviço acima indicado pelo período especificado "
         "contra defeitos de fabrico ou mão de obra.</p>\n";
    f << "<div class='rodape'>Emitido em " << dataAtual() << "</div>\n";
    f << "</body></html>\n";
    f.close();
    return filepath;
}

/* ================================================================
 * Apresentar opções: guardar / imprimir / ambos
 * ================================================================ */
void docApresentarOpcoes(const std::string& filepath, const std::string& tipo) {
    if (filepath.empty()) { std::cout << "  [!] Erro ao gerar documento.\n"; return; }

    std::cout << "\n  Documento gerado: " << filepath << "\n";
    std::cout << "  1. Guardar (já guardado)\n";
    std::cout << "  2. Imprimir\n";
    std::cout << "  3. Guardar e imprimir\n";
    std::cout << "  0. Ignorar\n";
    int op = lerInteiro("  Opção: ", 0, 3);

    if (op==2 || op==3) {
        // Tentar abrir no browser ou imprimir via lpr
        std::string cmd;
#ifdef __linux__
        cmd = "xdg-open \"" + filepath + "\" 2>/dev/null || lpr \"" + filepath + "\" 2>/dev/null";
#elif defined(__APPLE__)
        cmd = "open \"" + filepath + "\"";
#else
        cmd = "start \"\" \"" + filepath + "\"";
#endif
        std::system(cmd.c_str());
        std::cout << "  [OK] Documento enviado para visualização/impressão.\n";
    }
    if (op==1 || op==3) {
        std::cout << "  [OK] Ficheiro guardado em: " << filepath << "\n";
    }
}
