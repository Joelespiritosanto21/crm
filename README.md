# Sistema de Gestão — Loja de Eletrónica

Sistema CLI completo em C++17 para gestão de lojas de reparação e venda de equipamentos eletrónicos.
Sem dependências externas — usa apenas a biblioteca padrão C++.

---

## Funcionalidades

- **Autenticação** com username (3 letras) e password com hash SHA-256
- **Clientes** — criar, editar, listar, histórico, suspender (nunca apagar)
- **Produtos** — CRUD completo, pesquisa por EAN, alertas de stock baixo
- **Vendas / Faturas** — com geração automática de garantias e redução de stock
- **Orçamentos** — estados pendente/aprovado/rejeitado, conversão em fatura
- **Reparações** — fluxo completo de estados, garantia de 30 dias automática
- **Garantias** — criação automática, verificação por cliente
- **Lojas** — gestão multi-loja
- **Utilizadores** — roles: admin, gerente, vendedor, técnico
- **Documentos** — faturas e orçamentos em HTML prontos para imprimir
- **Logs** — registo de todas as ações

---

## Estrutura do Projeto

```
/projeto
├── main.cpp          — ponto de entrada e menu principal
├── auth.cpp/h        — autenticação e gestão de utilizadores
├── clientes.cpp/h    — gestão de clientes
├── produtos.cpp/h    — gestão de produtos e serviços
├── vendas.cpp/h      — vendas e faturas
├── orcamentos.cpp/h  — orçamentos
├── reparacoes.cpp/h  — reparações
├── garantias.cpp/h   — garantias
├── lojas.cpp/h       — lojas
├── logs.cpp/h        — sistema de logs
├── documentos.cpp/h  — geração de documentos HTML
├── json_utils.h      — parser/writer JSON (sem dependências externas)
├── sha256.h          — implementação SHA-256 pura em C++
├── common.h          — tipos partilhados, UI helpers, sessão global
├── Makefile
├── data/             — ficheiros JSON (base de dados)
│   ├── utilizadores.json
│   ├── clientes.json
│   ├── produtos.json
│   ├── vendas.json
│   ├── orcamentos.json
│   ├── reparacoes.json
│   ├── garantias.json
│   ├── lojas.json
│   └── logs.json
└── docs/             — documentos gerados (faturas, orçamentos, etc.)
```

---

## Compilação

### Pré-requisitos

- GCC / G++ com suporte a C++17 (`g++ --version` deve ser 7+)
- Sistema Linux/macOS (ou WSL no Windows)

### Com Makefile

```bash
make
```

### Manualmente

```bash
g++ -std=c++17 -O2 -o gestao \
  main.cpp auth.cpp clientes.cpp produtos.cpp vendas.cpp \
  orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
  logs.cpp documentos.cpp
```

---

## Execução

```bash
./gestao
```

Na primeira execução, o sistema cria automaticamente um administrador padrão:

```
Username : adm
Password : admin123
```

**Importante:** Altere a password após o primeiro login (menu Utilizadores → Editar).

---

## Credenciais e Roles

| Role      | Permissões                                              |
|-----------|--------------------------------------------------------|
| admin     | Controlo total, criar utilizadores, gerir lojas, apagar |
| gerente   | Tudo exceto gestão de utilizadores e lojas              |
| vendedor  | Clientes, produtos (leitura), vendas, orçamentos        |
| tecnico   | Reparações, garantias, clientes (leitura)               |

**Regra de username:** exatamente 3 letras (ex: `adm`, `joa`, `mar`)

---

## Fluxo de Venda

1. Pesquisar cliente por NIF / telefone / email
2. Se não existir → criar automaticamente
3. Adicionar produtos por EAN ou nome
4. Confirmar venda
5. **Automático:** stock reduzido + garantias criadas
6. Fatura gerada em `docs/FAT-XXXXXX.html`

---

## Fluxo de Reparação

```
recebido → diagnostico → reparacao → concluido → entregue
```

Ao marcar como **concluido**:
- Garantia de **30 dias** criada automaticamente
- Relatório de conclusão gerado em `docs/REP-XXXXXX_conclusao.html`

---

## Documentos Gerados

Todos os documentos são gerados em HTML em `/docs/`:

| Tipo       | Ficheiro                          |
|------------|-----------------------------------|
| Fatura     | `docs/FAT-000001.html`            |
| Orçamento  | `docs/ORC-000001.html`            |
| Entrada Rep| `docs/REP-000001_entrada.html`    |
| Conclusão  | `docs/REP-000001_conclusao.html`  |
| Garantia   | `docs/GAR_<id>.html`              |

Para imprimir: abrir no browser e usar Ctrl+P, ou usar `lpr`.

---

## Estrutura dos Ficheiros JSON

### utilizadores.json
```json
[
  {
    "id": "usr_...",
    "username": "adm",
    "password": "<sha256>",
    "nome": "Administrador",
    "role": "admin",
    "estado": "ativo",
    "loja_id": "",
    "criado_em": "2026-01-01 10:00:00"
  }
]
```

### clientes.json
```json
[
  {
    "id": "cli_...",
    "nome": "João Silva",
    "telefone": "912345678",
    "email": "joao@email.pt",
    "nif": "123456789",
    "estado": "ativo",
    "data_registo": "2026-01-10 09:00:00"
  }
]
```

### produtos.json
```json
[
  {
    "id": "prd_...",
    "nome": "iPhone 15 Pro 256GB",
    "descricao": "Apple iPhone 15 Pro",
    "preco": 1199.99,
    "stock": 5,
    "stock_minimo": 2,
    "ean": "0194253720027",
    "tipo": "produto",
    "tem_garantia": true,
    "duracao_garantia": 365,
    "ativo": true,
    "criado_em": "2026-01-01 10:00:00"
  }
]
```

### vendas.json
```json
[
  {
    "id": "vnd_...",
    "numero": "FAT-000001",
    "cliente_id": "cli_...",
    "itens": [
      {
        "produto_id": "prd_...",
        "nome": "iPhone 15 Pro 256GB",
        "ean": "0194253720027",
        "tipo": "produto",
        "quantidade": 1,
        "preco_unitario": 1199.99,
        "subtotal": 1199.99,
        "tem_garantia": true,
        "duracao_garantia": 365
      }
    ],
    "total": 1199.99,
    "data": "2026-04-01 10:30:00",
    "vendedor": "adm"
  }
]
```

### reparacoes.json
```json
[
  {
    "id": "rep_...",
    "numero": "REP-000001",
    "cliente_id": "cli_...",
    "equipamento": "iPhone 14",
    "marca": "Apple",
    "serie": "DNXXXXXX",
    "problema": "Ecrã partido",
    "acessorios": "Caixa",
    "senha": "1234",
    "estado": "reparacao",
    "tecnico": "tec",
    "orcamento": 120.00,
    "valor_final": 0,
    "data_entrada": "2026-04-01 09:00:00",
    "data_conclusao": "",
    "data_entrega": "",
    "notas": "",
    "garantia_id": ""
  }
]
```

### garantias.json
```json
[
  {
    "id": "gar_...",
    "cliente_id": "cli_...",
    "item": "iPhone 15 Pro 256GB",
    "referencia": "FAT-000001",
    "data_inicio": "2026-04-01",
    "data_fim": "2027-04-01",
    "duracao_dias": 365,
    "criado_em": "2026-04-01 10:30:00",
    "criado_por": "adm"
  }
]
```

---

## Segurança

- Passwords armazenadas com **SHA-256** (implementação própria, sem dependências)
- **Reautenticação** exigida para ações críticas (suspender cliente, desativar produto)
- Validação de inputs em todos os formulários
- Sem stock negativo
- EAN único por produto
- NIF único por cliente
- Clientes e produtos **nunca apagados** — apenas suspensos/desativados

---

## Notas Técnicas

- **Sem dependências externas** — parser JSON e SHA-256 implementados de raiz
- Toda a persistência é feita em ficheiros `.json` na pasta `data/`
- Sistema de ficheiros como "base de dados" — adequado para volumes pequenos/médios
- Compatível com Linux e macOS
- Para volumes grandes (>10.000 registos por ficheiro), considerar migração para SQLite

---

## Resolução de Problemas

**Erro de compilação "g++ não encontrado":**
```bash
sudo apt-get install g++   # Ubuntu/Debian
sudo yum install gcc-c++   # CentOS/RHEL
```

**Password esquecida do admin:**
Apagar `data/utilizadores.json` e reiniciar o programa. Um novo admin padrão será criado.

**Documentos não abrem automaticamente:**
Os ficheiros HTML ficam em `docs/`. Abrir manualmente no browser.
