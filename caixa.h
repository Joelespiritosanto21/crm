#pragma once
#include "json_utils.h"

void caixaMenu();
bool caixaAbrir(double fundo_inicial);
bool caixaFechar();
bool caixaRegistarPagamento(const std::string& metodo, double valor);
