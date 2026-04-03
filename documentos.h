#pragma once
/*
 * documentos.h - Geração de documentos (faturas, garantias, orçamentos)
 */

#include "json_utils.h"
#include "common.h"
#include <string>

/* Gerar e guardar documentos em /docs */
std::string docGerarFatura(const JsonValue& venda);
std::string docGerarOrcamento(const JsonValue& orcamento);
std::string docGerarGarantia(const JsonValue& garantia, const std::string& cliente_nome);

/* Apresentar opções ao utilizador (guardar/imprimir/ambos) */
void docApresentarOpcoes(const std::string& filepath, const std::string& tipo);

/* Criar diretório docs se não existir */
void docInicializar();
