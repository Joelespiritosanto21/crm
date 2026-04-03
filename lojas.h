#pragma once
/*
 * lojas.h - Gestão de lojas
 */

#include "json_utils.h"
#include "common.h"
#include <string>

void lojasCriar();
void lojasListar();
void lojasEditar();
void lojasMenu();
std::string lojaObterNome(const std::string& id);
