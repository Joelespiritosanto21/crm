#pragma once
/*
 * produtos.h - Gestão de produtos e serviços
 */

#include "json_utils.h"
#include "common.h"
#include <string>

void produtosCriar();
void produtosListar();
void produtosEditar();
void produtosApagar();
void produtosPesquisarEAN();
void produtosAlertasStock();
void produtosMenu();

/* Auxiliares para outros módulos */
JsonValue produtoEncontrarPorEAN(const std::string& ean);
JsonValue produtoEncontrarPorId(const std::string& id);
bool produtoReduzirStock(const std::string& produto_id, int quantidade);
void produtosAtualizarStock(const std::string& produto_id, int delta); // delta pode ser negativo
