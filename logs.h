#pragma once
/*
 * logs.h - Sistema de logs
 */

#include <string>
#include "json_utils.h"
#include "common.h"

void logRegistar(const std::string& acao, const std::string& detalhe = "");
void logsListar();
