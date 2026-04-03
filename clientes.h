#pragma once
/*
 * clientes.h - Gestão de clientes
 */

#include "json_utils.h"
#include "common.h"
#include <string>
#include <optional>

void clientesCriar();
void clientesListar();
void clientesEditar();
void clientesHistorico();
void clientesSuspender();
void clientesMenu();
void clientesPesquisar();

/* Funções auxiliares usadas por outros módulos */
JsonValue clienteEncontrar(const std::string& campo, const std::string& valor);
JsonValue clienteEncontrarOuCriar(); // Fluxo de venda
std::string clienteObterNome(const std::string& id);
