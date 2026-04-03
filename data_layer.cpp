#include "data_layer.h"
#include "api_client.h"
#include "common.h"

#include <sys/stat.h>

JsonValue dl_get_clientes_local() {
    return jsonParseFile(FILE_CLIENTES);
}

bool dl_save_clientes_local(const JsonValue& clientes) {
    return jsonSaveFile(FILE_CLIENTES, clientes);
}

bool dl_add_cliente_local(const JsonValue& cliente) {
    JsonValue clientes = dl_get_clientes_local();
    JsonArray arr;
    if (clientes.isArray()) arr = clientes.arr;
    arr.push_back(cliente);
    JsonValue novo(arr);
    return dl_save_clientes_local(novo);
}

bool dl_pull_clientes_from_server(const std::string& host, int port) {
    JsonValue clientes;
    if (!api_get_clientes(host, port, clientes)) return false;
    // Se resposta for objeto com campo "clientes", extrair
    if (clientes.isObject() && clientes.has("clientes")) {
        JsonValue c = clientes["clientes"];
        if (c.isArray()) return dl_save_clientes_local(c);
        return false;
    }
    if (clientes.isArray()) return dl_save_clientes_local(clientes);
    return false;
}

// Produtos
JsonValue dl_get_produtos_local() {
    return jsonParseFile(FILE_PRODUTOS);
}

bool dl_save_produtos_local(const JsonValue& produtos) {
    return jsonSaveFile(FILE_PRODUTOS, produtos);
}

bool dl_add_produto_local(const JsonValue& produto) {
    JsonValue prods = dl_get_produtos_local();
    JsonArray arr;
    if (prods.isArray()) arr = prods.arr;
    arr.push_back(produto);
    return dl_save_produtos_local(JsonValue(arr));
}

// Vendas
JsonValue dl_get_vendas_local() {
    return jsonParseFile(FILE_VENDAS);
}

bool dl_save_vendas_local(const JsonValue& vendas) {
    return jsonSaveFile(FILE_VENDAS, vendas);
}

bool dl_add_venda_local(const JsonValue& venda) {
    JsonValue vendas = dl_get_vendas_local();
    JsonArray arr;
    if (vendas.isArray()) arr = vendas.arr;
    arr.push_back(venda);
    return dl_save_vendas_local(JsonValue(arr));
}
