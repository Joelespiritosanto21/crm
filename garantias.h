#pragma once
/*
 * garantias.h - Gestão de garantias
 */

#include "json_utils.h"
#include "common.h"
#include <string>

void garantiasListar();
void garantiasVerificar();
void garantiasMenu();

/* Criar garantia automaticamente (chamado por vendas e reparações) */
std::string garantiaCriarAuto(const std::string& cliente_id,
                               const std::string& item_nome,
                               int duracao_dias,
                               const std::string& referencia = "");
