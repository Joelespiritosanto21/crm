#pragma once
/*
 * auth.h - Autenticação e gestão de utilizadores
 */

#include <string>
#include "json_utils.h"
#include "common.h"

/* Login / logout */
bool authLogin();
void authLogout();
bool authReautenticar(); // Para ações críticas

/* Gestão de utilizadores (admin) */
void utilizadoresCriar();
void utilizadoresListar();
void utilizadoresEditar();
void utilizadoresMenu();

/* Inicialização (cria admin padrão se não existir) */
void authInicializar();
