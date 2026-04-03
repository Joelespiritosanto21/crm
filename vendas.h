#pragma once
/*
 * vendas.h - Gestão de vendas e faturas
 */

#include "json_utils.h"
#include "common.h"
#include <string>

void vendasCriar();
void vendasListar();
void vendasDetalhe();
void vendasMenu();

/* Retorna próximo número sequencial de fatura */
int vendasProximoNumero();
