# TechFix — Sistema de Gestão de Loja de Eletrónica

Sistema CLI completo em C++11 para lojas de reparação e venda de eletrónica.
**Sem dependências externas** — JSON, SHA-256 e servidor WebSocket implementados de raiz.

---

## Funcionalidades

| Módulo                       | Descrição                                                          |
| ---------------------------- | ------------------------------------------------------------------ |
| Autenticação                 | Login com SHA-256, username 3 letras, reautenticação crítica       |
| Clientes                     | Criar, editar, histórico, suspender (nunca apagar)                 |
| Produtos                     | CRUD, EAN único, alertas de stock, serviços                        |
| Vendas/Faturas               | Stock automático, garantias automáticas, documentos HTML           |
| Orçamentos                   | Estados, conversão em fatura                                       |
| Reparações                   | Fluxo de estados, garantia 30 dias auto                            |
| Garantias                    | Criação automática, verificação por cliente                        |
| Lojas                        | Multi-loja                                                         |
| Utilizadores                 | Roles: admin, gerente, vendedor, técnico                           |
| Logs                         | Auditoria completa                                                 |
| **Servidor Web**             | **Terminal no browser, porta 2021**                                |
| **Sincronização Multi-loja** | `server_api` (porta 2022) centraliza clientes, vendas e encomendas |

---

## Multi-loja & Sincronização

O sistema suporta agora um modo multi-loja com sincronização básica cliente/servidor:

- `server_api` — binário simples TCP/JSON que corre num VPS (porta 2022 por omissão) e guarda dados em `server_data/`.
- `gestao` (loja) — cada loja enfileira operações em `data/sync_queue.json` e pode sincronizar (menu `Z`).
- Operações suportadas: `create_cliente`, `create_produto`, `create_venda`, `create_transferencia`, `create_fornecedor`, `create_encomenda`.

Exemplo: iniciar servidor central e loja local

```bash
# no VPS
./server_api

# em cada loja
./gestao
```

Sincronização: `Z → Sincronizar com servidor` (menu principal) — envia a fila e faz pull de clientes.

### Autenticação entre loja e servidor

- O servidor aceita um *token* simples (bearer-like) para autenticar pedidos.
- Inicie o servidor com o token como segundo argumento ou defina a variável de ambiente `API_TOKEN`:

```bash
# Porta 2022 e token "devtoken"
./server_api 2022 devtoken
# ou (env)
API_TOKEN=devtoken ./server_api 2022
```

- Na loja (`gestao`), abra `Z → Sincronizar com servidor → 4) Definir token API` e cole o mesmo token (`devtoken`).
- O cliente anexa automaticamente o campo `token` às requests JSON antes de as enviar.


---

## Fase 2 — Funcionalidades de negócio adicionadas

- **Caixa diária** (`Z` → `K` / menu Caixa): abertura/fecho de caixa, registo de pagamentos por método (dinheiro/MB/cartão/crédito).
- **Devoluções**: registo de devolução, reposição de stock e nota de crédito básica.
- **Transferências entre lojas**: pedido de transferência entre lojas (registado em `data/transferencias.json`).
- **Fornecedores**: CRUD básico em `data/fornecedores.json`.
- **Encomendas a fornecedores**: criar encomenda manual ou a partir dos alertas de stock (gera `data/encomendas.json`).
- **Relatórios**: vendas por período e top produtos.
- **Fidelização**: pontos por compra (1 ponto por euro) são acumulados no registo de cliente.

---

## UI — Ecrã Fixo 80 Colunas

Cada ecrã tem sempre o mesmo layout:

```
╔══════════════════════════════════════════════════════════════════════════════╗
║ TECHFIX │ MENU PRINCIPAL                             adm [admin]  14:32:01  ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║  +--------------------------------------+                                    ║
║  |   GESTAO DE LOJA DE ELECTRONICA      |                                    ║
║  +--------------------------------------+                                    ║
║   1.  Dashboard                                                              ║
║   2.  Clientes                                                               ║
║   3.  Produtos                                                               ║
║   ...                                                                        ║
╠══════════════════════════════════════════════════════════════════════════════╣
║ Menu Principal                               [ Introduza o numero da opcao ] ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

- **Topo:** sistema, título do ecrã, utilizador, hora
- **Meio:** conteúdo com bordas laterais
- **Fundo:** breadcrumb de navegação + dica de teclas
- Ecrã limpo a cada mudança de secção — sem scroll acumulado

---

## Compilação

### Pré-requisitos

- g++ com C++11 (`g++ --version` deve ser 4.8+)
- Linux/macOS para o servidor web (usa PTY e fork)
- Windows: apenas o terminal `gestao.exe` (sem servidor web)

### Compilar tudo (Linux/macOS)

```bash
make
```

### Compilar manualmente

```bash
# Terminal CLI
g++ -std=c++11 -O2 -o gestao \
  main.cpp auth.cpp clientes.cpp produtos.cpp vendas.cpp \
  orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
  logs.cpp documentos.cpp

# Servidor web (Linux/macOS apenas)
g++ -std=c++11 -O2 -o webserver webserver.cpp -lpthread
```

### Windows (MinGW / g++ nativo)

```bat
g++ -std=c++11 -O2 -o gestao.exe main.cpp auth.cpp clientes.cpp ^
    produtos.cpp vendas.cpp orcamentos.cpp reparacoes.cpp ^
    garantias.cpp lojas.cpp logs.cpp documentos.cpp
```

_(O servidor web não funciona no Windows nativo — usar WSL)_

---

## Execução

### Terminal direto

```bash
./gestao
# ou Windows:
gestao.exe
```

### Servidor web (porta 2021)

```bash
./webserver
# Abrir no browser: http://localhost:2021
```

O servidor cria um terminal interativo no browser com:

- **xterm.js** — emulador de terminal completo
- **WebSocket** — comunicação em tempo real
- **PTY** — pseudo-terminal ligado ao processo `gestao`
- Suporte a ANSI, UTF-8, box-drawing characters

### Script de arranque

```bash
./start.sh        # terminal direto
./start.sh web    # servidor web
```

---

## Credenciais iniciais

Na primeira execução é criado automaticamente:

| Campo    | Valor      |
| -------- | ---------- |
| Username | `adm`      |
| Password | `admin123` |
| Role     | admin      |

**Altere a password após o primeiro login** (menu Utilizadores → Editar).

**Regra de username:** exatamente 3 letras (ex: `adm`, `joa`, `tec`)

---

## Roles e permissões

| Role       | Permissões                                       |
| ---------- | ------------------------------------------------ |
| `admin`    | Controlo total                                   |
| `gerente`  | Tudo exceto gestão de utilizadores/lojas         |
| `vendedor` | Clientes, vendas, orçamentos, produtos (leitura) |
| `tecnico`  | Reparações, garantias, clientes (leitura)        |

---

## Fluxo de Venda (automações)

1. Pesquisar cliente → criar se não existir
2. Adicionar produtos por EAN ou nome
3. Confirmar venda
4. **Automático:**
   - Stock reduzido por item
   - Garantias criadas para items com garantia
   - Fatura HTML gerada em `docs/FAT-XXXXXX.html`

---

## Fluxo de Reparação

```
recebido → diagnostico → reparacao → concluido → entregue
```

Ao concluir → garantia de **30 dias** criada automaticamente.

---

## Estrutura de ficheiros

```
/projeto
├── main.cpp          — menu principal e dashboard
├── auth.cpp/h        — autenticação
├── clientes.cpp/h    — clientes
├── produtos.cpp/h    — produtos e serviços
├── vendas.cpp/h      — vendas e faturas
├── orcamentos.cpp/h  — orçamentos
├── reparacoes.cpp/h  — reparações
├── garantias.cpp/h   — garantias
├── lojas.cpp/h       — lojas
├── logs.cpp/h        — logs
├── documentos.cpp/h  — geração de documentos HTML
├── webserver.cpp     — servidor web porta 2021 (Linux/macOS)
├── common.h          — TUI de ecrã fixo, input, layout
├── json_utils.h      — parser/writer JSON sem dependências
├── sha256.h          — SHA-256 puro C++
├── Makefile
├── start.sh          — script de arranque
├── data/             — base de dados JSON
└── docs/             — documentos gerados
```

---

## Resolução de problemas

**"g++ não encontrado"**

```bash
sudo apt-get install g++ build-essential   # Ubuntu/Debian
sudo yum install gcc-c++                   # CentOS/RHEL
```

**Servidor web: "bind: Address already in use"**

```bash
pkill webserver
./webserver
```

**Password do admin perdida**
Apagar `data/utilizadores.json` e reiniciar — novo admin criado automaticamente.

**Box-drawing no Windows CMD**
Executar `chcp 65001` antes de correr o programa, ou usar Windows Terminal.
