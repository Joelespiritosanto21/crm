#pragma once
/*
 * protocolo.h - Protocolo TCP entre gestao_server e gestao_loja
 *
 * Cada mensagem e uma linha JSON terminada em \n
 * Request:  {"cmd":"LOGIN","data":{...}}
 * Response: {"ok":true,"data":{...}}  ou  {"ok":false,"erro":"..."}
 *
 * Porta padrao do servidor de lojas: 2022
 */

#define PROTO_PORT   2022
#define PROTO_MAXBUF 65536

/* ── Comandos disponiveis ─────────────────────────────────── */

/* Auth */
#define CMD_LOGIN            "LOGIN"
#define CMD_LOGOUT           "LOGOUT"

/* Clientes */
#define CMD_CLI_CRIAR        "CLI_CRIAR"
#define CMD_CLI_BUSCAR       "CLI_BUSCAR"
#define CMD_CLI_LISTAR       "CLI_LISTAR"
#define CMD_CLI_EDITAR       "CLI_EDITAR"
#define CMD_CLI_SUSPENDER    "CLI_SUSPENDER"
#define CMD_CLI_HISTORICO    "CLI_HISTORICO"

/* Produtos */
#define CMD_PRD_CRIAR        "PRD_CRIAR"
#define CMD_PRD_LISTAR       "PRD_LISTAR"
#define CMD_PRD_BUSCAR_EAN   "PRD_BUSCAR_EAN"
#define CMD_PRD_BUSCAR_NOME  "PRD_BUSCAR_NOME"
#define CMD_PRD_EDITAR       "PRD_EDITAR"
#define CMD_PRD_STOCK_BAIXO  "PRD_STOCK_BAIXO"
#define CMD_PRD_TRANS_STOCK  "PRD_TRANS_STOCK"  /* transferencia entre lojas */

/* Vendas */
#define CMD_VND_CRIAR        "VND_CRIAR"
#define CMD_VND_LISTAR       "VND_LISTAR"
#define CMD_VND_DETALHE      "VND_DETALHE"

/* Devolucoes */
#define CMD_DEV_CRIAR        "DEV_CRIAR"
#define CMD_DEV_LISTAR       "DEV_LISTAR"

/* Orcamentos */
#define CMD_ORC_CRIAR        "ORC_CRIAR"
#define CMD_ORC_LISTAR       "ORC_LISTAR"
#define CMD_ORC_ESTADO       "ORC_ESTADO"
#define CMD_ORC_CONVERTER    "ORC_CONVERTER"

/* Reparacoes */
#define CMD_REP_CRIAR        "REP_CRIAR"
#define CMD_REP_LISTAR       "REP_LISTAR"
#define CMD_REP_ESTADO       "REP_ESTADO"
#define CMD_REP_CONCLUIR     "REP_CONCLUIR"

/* Garantias */
#define CMD_GAR_LISTAR       "GAR_LISTAR"
#define CMD_GAR_VERIFICAR    "GAR_VERIFICAR"

/* Caixa */
#define CMD_CX_ABRIR         "CX_ABRIR"
#define CMD_CX_FECHAR        "CX_FECHAR"
#define CMD_CX_ESTADO        "CX_ESTADO"
#define CMD_CX_MOVIMENTOS    "CX_MOVIMENTOS"

/* Fornecedores */
#define CMD_FORN_CRIAR       "FORN_CRIAR"
#define CMD_FORN_LISTAR      "FORN_LISTAR"
#define CMD_FORN_ENCOMENDA   "FORN_ENCOMENDA"

/* Relatorios */
#define CMD_REL_VENDAS       "REL_VENDAS"
#define CMD_REL_CAIXA        "REL_CAIXA"
#define CMD_REL_STOCK        "REL_STOCK"
#define CMD_REL_REPARACOES   "REL_REPARACOES"
#define CMD_REL_TOP_PRODUTOS "REL_TOP_PRODUTOS"
#define CMD_REL_VENDEDOR     "REL_VENDEDOR"

/* Utilizadores (admin) */
#define CMD_USR_CRIAR        "USR_CRIAR"
#define CMD_USR_LISTAR       "USR_LISTAR"
#define CMD_USR_EDITAR       "USR_EDITAR"

/* Lojas (admin) */
#define CMD_LOJ_CRIAR        "LOJ_CRIAR"
#define CMD_LOJ_LISTAR       "LOJ_LISTAR"
#define CMD_LOJ_EDITAR       "LOJ_EDITAR"

/* Fidelizacao */
#define CMD_FID_SALDO        "FID_SALDO"
#define CMD_FID_ADICIONAR    "FID_ADICIONAR"
#define CMD_FID_RESGATAR     "FID_RESGATAR"

/* Logs */
#define CMD_LOG_LISTAR       "LOG_LISTAR"

/* Dashboard */
#define CMD_DASHBOARD        "DASHBOARD"
