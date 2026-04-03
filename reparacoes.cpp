/*
 * reparacoes.cpp - Implementação de gestão de reparações
 */

#include "reparacoes.h"
#include "clientes.h"
#include "garantias.h"
#include "documentos.h"
#include "logs.h"
#include <iostream>
#include <iomanip>

/* Estados válidos */
static const std::vector<std::string> ESTADOS = {
    "recebido", "diagnostico", "reparacao", "concluido", "entregue"
};

static std::string estadoSeguinte(const std::string& atual) {
    for (size_t i=0; i<ESTADOS.size()-1; ++i)
        if (ESTADOS[i]==atual) return ESTADOS[i+1];
    return atual;
}

static int reparacoesProximoNumero() {
    JsonValue reps = jsonParseFile(FILE_REPARACOES);
    if (!reps.isArray() || reps.arr.empty()) return 1;
    int max = 0;
    for (auto& r : reps.arr) {
        std::string num = r["numero"].asString();
        if (num.size() > 4) {
            try { int n = std::stoi(num.substr(4)); if (n>max) max=n; } catch(...) {}
        }
    }
    return max + 1;
}

/* ================================================================
 * Criar reparação
 * ================================================================ */
void reparacoesCriar() {
    titulo("NOVA REPARAÇÃO");

    JsonValue cliente = clienteEncontrarOuCriar();
    if (cliente.isNull()) { std::cout << "  Cancelado.\n"; return; }

    std::string cliente_id   = cliente["id"].asString();
    std::string cliente_nome = cliente["nome"].asString();
    std::cout << "\n  Cliente: " << cliente_nome << "\n";

    std::string equipamento = lerString("  Equipamento (ex: iPhone 14, Samsung TV): ");
    if (equipamento.empty()) { std::cout << "  Obrigatório.\n"; return; }

    std::string marca    = lerString("  Marca/Modelo: ");
    std::string serie    = lerString("  Nº série / IMEI (opcional): ");
    std::string problema = lerString("  Descrição do problema: ");
    if (problema.empty()) { std::cout << "  Obrigatório.\n"; return; }

    std::string acessorios = lerString("  Acessórios entregues (opcional): ");
    std::string senha      = lerString("  Senha/PIN do equipamento (opcional): ");

    double orcamento_valor = 0.0;
    if (lerSimNao("  Registar orçamento inicial?")) {
        orcamento_valor = lerDouble("  Valor estimado: ");
    }

    std::string tecnico = g_sessao.username;
    // Admin/Gerente pode atribuir a outro técnico
    if (temPermissao("gerente")) {
        std::string tec = lerString("  Técnico responsável (" + tecnico + "): ");
        if (!tec.empty()) tecnico = tec;
    }

    int seq = reparacoesProximoNumero();
    std::string numero = gerarNumeroReparacao(seq);

    JsonObject rep;
    rep["id"]              = JsonValue(generateId("rep_"));
    rep["numero"]          = JsonValue(numero);
    rep["cliente_id"]      = JsonValue(cliente_id);
    rep["equipamento"]     = JsonValue(equipamento);
    rep["marca"]           = JsonValue(marca);
    rep["serie"]           = JsonValue(serie);
    rep["problema"]        = JsonValue(problema);
    rep["acessorios"]      = JsonValue(acessorios);
    rep["senha"]           = JsonValue(senha);
    rep["estado"]          = JsonValue(std::string("recebido"));
    rep["tecnico"]         = JsonValue(tecnico);
    rep["orcamento"]       = JsonValue(orcamento_valor);
    rep["data_entrada"]    = JsonValue(dataAtual());
    rep["data_conclusao"]  = JsonValue(std::string(""));
    rep["data_entrega"]    = JsonValue(std::string(""));
    rep["notas"]           = JsonValue(std::string(""));
    rep["garantia_id"]     = JsonValue(std::string(""));

    JsonValue reps = jsonParseFile(FILE_REPARACOES);
    if (!reps.isArray()) reps = JsonValue(JsonArray{});
    reps.arr.push_back(JsonValue(rep));
    jsonSaveFile(FILE_REPARACOES, reps);

    logRegistar("criar_reparacao", "numero=" + numero + " cliente=" + cliente_nome + " equip=" + equipamento);

    std::cout << "\n  [OK] Reparação " << numero << " criada.\n";
    std::cout << "  Estado inicial: recebido\n";
    std::cout << "  Técnico: " << tecnico << "\n";

    // Gerar comprovativo de entrada (texto simples)
    std::string filepath = std::string(DOCS_DIR) + numero + "_entrada.html";
    std::ofstream f(filepath);
    if (f.is_open()) {
        f << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Comprovativo " << numero << "</title>"
          << "<style>body{font-family:Arial;max-width:600px;margin:40px auto;padding:20px}"
          << "h1{color:#1a237e}</style></head><body>\n"
          << "<h1>Comprovativo de Entrega para Reparação</h1>\n"
          << "<p><b>Nº:</b> " << numero << "</p>\n"
          << "<p><b>Data:</b> " << dataAtual() << "</p>\n"
          << "<p><b>Cliente:</b> " << cliente_nome << "</p>\n"
          << "<p><b>Equipamento:</b> " << equipamento << " " << marca << "</p>\n"
          << "<p><b>Nº série:</b> " << serie << "</p>\n"
          << "<p><b>Problema:</b> " << problema << "</p>\n"
          << "<p><b>Acessórios:</b> " << acessorios << "</p>\n";
        if (orcamento_valor > 0)
            f << "<p><b>Orçamento estimado:</b> " << std::fixed << std::setprecision(2) << orcamento_valor << " EUR</p>\n";
        f << "<p><b>Técnico:</b> " << tecnico << "</p>\n"
          << "<hr><p style='font-size:12px'>Este documento serve de comprovativo da entrega do equipamento para reparação.</p>\n"
          << "</body></html>\n";
        f.close();
        docApresentarOpcoes(filepath, "Comprovativo");
    }
}

/* ================================================================
 * Listar reparações
 * ================================================================ */
void reparacoesListar() {
    JsonValue reps = jsonParseFile(FILE_REPARACOES);
    subtitulo("LISTA DE REPARAÇÕES");
    if (!reps.isArray() || reps.arr.empty()) { std::cout << "  Sem reparações.\n"; return; }

    // Filtro de estado
    std::cout << "  Filtrar: 1=Todas  2=Em curso  3=Concluídas  4=Por técnico\n";
    int filtro = lerInteiro("  Opção: ", 1, 4);
    std::string tec_filtro;
    if (filtro==4) tec_filtro = lerString("  Username do técnico: ");

    std::cout << "\n" << std::left
              << std::setw(12) << "NÚMERO"
              << std::setw(20) << "CLIENTE"
              << std::setw(22) << "EQUIPAMENTO"
              << std::setw(14) << "ESTADO"
              << "TÉCNICO\n";
    linha();

    for (auto& r : reps.arr) {
        std::string estado = r["estado"].asString();
        if (filtro==2 && (estado=="concluido"||estado=="entregue")) continue;
        if (filtro==3 && estado!="concluido") continue;
        if (filtro==4 && r["tecnico"].asString()!=tec_filtro) continue;

        std::string cn = clienteObterNome(r["cliente_id"].asString());
        std::cout << std::setw(12) << r["numero"].asString()
                  << std::setw(20) << cn.substr(0,19)
                  << std::setw(22) << (r["equipamento"].asString()+" "+r["marca"].asString()).substr(0,21)
                  << std::setw(14) << estado
                  << r["tecnico"].asString() << "\n";
    }
}

/* ================================================================
 * Atualizar estado da reparação
 * ================================================================ */
void reparacoesAtualizarEstado() {
    subtitulo("ATUALIZAR ESTADO DA REPARAÇÃO");
    std::string num = lerString("  Número da reparação (ex: REP-000001): ");

    JsonValue reps = jsonParseFile(FILE_REPARACOES);
    if (!reps.isArray()) return;

    for (auto& r : reps.arr) {
        if (r["numero"].asString()==num) {
            std::string atual    = r["estado"].asString();
            std::string seguinte = estadoSeguinte(atual);

            std::cout << "  Reparação: " << r["equipamento"].asString()
                      << " (" << clienteObterNome(r["cliente_id"].asString()) << ")\n";
            std::cout << "  Estado atual: " << atual << "\n\n";

            std::cout << "  Estados:\n";
            for (size_t i=0; i<ESTADOS.size(); ++i)
                std::cout << "  " << (i+1) << ". " << ESTADOS[i]
                          << (ESTADOS[i]==atual ? "  <- atual" : "") << "\n";
            std::cout << "  0. Cancelar\n";

            int op = lerInteiro("  Novo estado (número): ", 0, (int)ESTADOS.size());
            if (op==0) return;
            std::string novo_estado = ESTADOS[op-1];

            if (novo_estado == "concluido") {
                // Usar a função de conclusão completa
                r["estado"]         = JsonValue(novo_estado);
                r["data_conclusao"] = JsonValue(dataAtual());
                jsonSaveFile(FILE_REPARACOES, reps);
                reparacoesConcluir(); // Chamar fluxo de conclusão
                return;
            }

            std::string notas = lerString("  Notas (opcional): ");
            r["estado"] = JsonValue(novo_estado);
            if (!notas.empty()) {
                std::string n_existente = r["notas"].asString();
                r["notas"] = JsonValue(n_existente + "\n[" + dataAtual() + "] " + notas);
            }

            jsonSaveFile(FILE_REPARACOES, reps);
            logRegistar("atualizar_reparacao", "numero=" + num + " estado=" + novo_estado);
            std::cout << "  [OK] Estado atualizado: " << atual << " → " << novo_estado << "\n";
            return;
        }
    }
    std::cout << "  [!] Reparação não encontrada.\n";
}

/* ================================================================
 * Concluir reparação (com garantia automática de 30 dias)
 * ================================================================ */
void reparacoesConcluir() {
    subtitulo("CONCLUIR REPARAÇÃO");
    std::string num = lerString("  Número da reparação: ");

    JsonValue reps = jsonParseFile(FILE_REPARACOES);
    if (!reps.isArray()) return;

    for (auto& r : reps.arr) {
        if (r["numero"].asString()==num) {
            if (r["estado"].asString()=="entregue") {
                std::cout << "  [!] Reparação já foi entregue.\n"; return;
            }

            std::cout << "  Cliente: " << clienteObterNome(r["cliente_id"].asString()) << "\n";
            std::cout << "  Equipamento: " << r["equipamento"].asString() << "\n\n";

            double valor_final = lerDouble("  Valor final da reparação (EUR): ");
            std::string diagnostico = lerString("  Diagnóstico/trabalho realizado: ");
            std::string pecas       = lerString("  Peças substituídas (opcional): ");

            if (!lerSimNao("  Confirmar conclusão da reparação?")) return;

            // Atualizar campos
            r["estado"]         = JsonValue(std::string("concluido"));
            r["data_conclusao"] = JsonValue(dataAtual());
            r["valor_final"]    = JsonValue(valor_final);
            std::string notas   = r["notas"].asString();
            notas += "\n[CONCLUSÃO " + dataAtual() + "] " + diagnostico;
            if (!pecas.empty()) notas += " | Peças: " + pecas;
            r["notas"] = JsonValue(notas);

            // Criar garantia automática de 30 dias
            std::string gar_id = garantiaCriarAuto(
                r["cliente_id"].asString(),
                "Reparação: " + r["equipamento"].asString() + " " + r["marca"].asString(),
                30,
                num
            );
            r["garantia_id"] = JsonValue(gar_id);

            jsonSaveFile(FILE_REPARACOES, reps);
            logRegistar("concluir_reparacao", "numero=" + num + " valor=" + std::to_string(valor_final));

            std::cout << "\n  [OK] Reparação concluída!\n";
            std::cout << "  Garantia de 30 dias criada automaticamente.\n";
            std::cout << "  Valor: " << std::fixed << std::setprecision(2) << valor_final << " EUR\n";

            // Gerar documento de entrega
            std::string filepath = std::string(DOCS_DIR) + num + "_conclusao.html";
            std::ofstream f(filepath);
            if (f.is_open()) {
                f << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Conclusão " << num << "</title>"
                  << "<style>body{font-family:Arial;max-width:600px;margin:40px auto;padding:20px}"
                  << "h1{color:#1a237e}</style></head><body>\n"
                  << "<h1>Relatório de Reparação - " << num << "</h1>\n"
                  << "<p><b>Data:</b> " << dataAtual() << "</p>\n"
                  << "<p><b>Cliente:</b> " << clienteObterNome(r["cliente_id"].asString()) << "</p>\n"
                  << "<p><b>Equipamento:</b> " << r["equipamento"].asString() << " " << r["marca"].asString() << "</p>\n"
                  << "<p><b>Problema:</b> " << r["problema"].asString() << "</p>\n"
                  << "<p><b>Trabalho realizado:</b> " << diagnostico << "</p>\n";
                if (!pecas.empty()) f << "<p><b>Peças:</b> " << pecas << "</p>\n";
                f << "<p><b>Valor:</b> " << std::fixed << std::setprecision(2) << valor_final << " EUR</p>\n"
                  << "<p><b>Garantia:</b> 30 dias (até " << adicionarDias(dataApenasData(),30) << ")</p>\n"
                  << "<p><b>Técnico:</b> " << r["tecnico"].asString() << "</p>\n"
                  << "</body></html>\n";
                f.close();
                docApresentarOpcoes(filepath, "Relatório de reparação");
            }
            return;
        }
    }
    std::cout << "  [!] Reparação não encontrada.\n";
}

/* ================================================================
 * Menu reparações
 * ================================================================ */
void reparacoesMenu() {
    while (true) {
        titulo("GESTÃO DE REPARAÇÕES");
        std::cout << "  1. Nova reparação\n";
        std::cout << "  2. Listar reparações\n";
        std::cout << "  3. Atualizar estado\n";
        std::cout << "  4. Concluir reparação\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 4);
        if (op==0) break;
        else if (op==1) reparacoesCriar();
        else if (op==2) reparacoesListar();
        else if (op==3) reparacoesAtualizarEstado();
        else if (op==4) reparacoesConcluir();
        pausar();
    }
}
