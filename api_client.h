#pragma once
#include "json_utils.h"
#include <string>

// Envia uma requisicao JSON para o servidor (socket TCP) e espera resposta JSON terminada por '\n'
bool api_send_request(const std::string& host, int port, const JsonValue& req, JsonValue& resp, int timeout_ms=5000);

bool api_get_clientes(const std::string& host, int port, JsonValue& clientes);
bool api_post_cliente(const std::string& host, int port, const JsonValue& cliente, JsonValue& resp);
