#pragma once
#include "json_utils.h"
#include <string>

// Camada de acesso a dados (local/remote helpers)
JsonValue dl_get_clientes_local();
bool dl_save_clientes_local(const JsonValue& clientes);
bool dl_add_cliente_local(const JsonValue& cliente);

// Sincronizacao simples (pull do servidor sobrescreve o local)
bool dl_pull_clientes_from_server(const std::string& host, int port);

// Produtos
JsonValue dl_get_produtos_local();
bool dl_save_produtos_local(const JsonValue& produtos);
bool dl_add_produto_local(const JsonValue& produto);

// Vendas
JsonValue dl_get_vendas_local();
bool dl_save_vendas_local(const JsonValue& vendas);
bool dl_add_venda_local(const JsonValue& venda);
