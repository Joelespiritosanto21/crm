# TechFix — Sistema de Gestão Multi-Loja

Sistema profissional para lojas de reparação e venda de eletrónica.
Arquitectura cliente/servidor: dados centralizados, múltiplas lojas em simultâneo.

---

## Arquitectura

```
VPS/Servidor
└── gestao_server  (porta 2022 = lojas | porta 2021 = web browser)
    └── data/  ← base de dados JSON central

PC Loja Guimarães          PC Loja Porto
└── gestao_loja            └── gestao_loja
    └── servidor.conf          └── servidor.conf
```

- **Sem modo offline** — sem servidor = sem acesso (intencional)
- Cada utilizador está ligado a uma loja específica
- Um vendedor da loja A não consegue aceder à loja B

---

## Compilar

### Linux / macOS (servidor + clientes)
```bash
make
```

### Manualmente
```bash
# Servidor
g++ -std=c++11 -O2 -o gestao_server gestao_server.cpp -lpthread -lutil

# Cliente das lojas
g++ -std=c++11 -O2 -o gestao_loja gestao_loja.cpp -lpthread
```

### Windows (apenas cliente)
```bat
g++ -std=c++11 -O2 -o gestao_loja.exe gestao_loja.cpp -lws2_32
```
*(o servidor requer Linux — usar WSL ou VPS)*

---

## Instalar o Servidor (VPS)

### Início rápido
```bash
./gestao_server
# Ctrl+C para parar
```

### Como serviço permanente (systemd)
```bash
sudo ./install_server.sh
# O servidor inicia automaticamente com o sistema
# Logs: sudo journalctl -u techfix -f
```

### Abrir portas na firewall
```bash
sudo ./firewall.sh        # abre portas 2021 e 2022
```

### Credenciais iniciais
| Campo    | Valor      |
|----------|------------|
| Username | `adm`      |
| Password | `admin123` |
| Role     | admin      |

**Mudar a password após o primeiro login** (menu S → Utilizadores → Editar).

---

## Configurar um PC de Loja

1. Copiar `gestao_loja` para o PC da loja
2. Criar ficheiro `servidor.conf` na mesma pasta:
   ```ini
   host=45.131.64.57     # IP do teu servidor
   port=2022
   loja_id=loj_xxxxxxxx  # ID atribuído no sistema
   ```
3. Executar:
   ```bash
   ./gestao_loja
   # ou Windows:
   gestao_loja.exe
   ```

Alternativamente, na primeira execução sem `servidor.conf`, o programa pergunta os dados automaticamente.

---

## Funcionalidades

### Vendas
- Nova venda com produtos por EAN ou pesquisa de nome
- Múltiplos métodos de pagamento: Dinheiro, MB, Cartão, MBWay, Crédito, Cheque
- Desconto por item em percentagem
- Garantias criadas automaticamente após venda
- Stock debitado automaticamente por loja
- Pontos de fidelização atribuídos (1 ponto por cada 10 EUR)

### Caixa Diária
- Abertura com fundo de caixa
- Todos os movimentos registados automaticamente
- Fecho com contagem e cálculo de diferença

### Devoluções
- Devolução de qualquer fatura anterior
- Stock reposto automaticamente
- Múltiplos métodos de reembolso

### Reparações
- Fluxo: `recebido → diagnóstico → reparação → concluído → entregue`
- Garantia de 30 dias criada automaticamente ao concluir
- Atribuição por técnico

### Orçamentos
- Estados: pendente → aprovado → convertido em fatura
- Conversão direta em fatura com um clique

### Garantias
- Criação automática (vendas e reparações)
- Verificação rápida por NIF ou telefone

### Fidelização
- 1 ponto por cada 10 EUR gastos
- Resgate: 10 pontos = 5 EUR de desconto (máximo 50% da venda)

### Relatórios (gerente+)
- Vendas por período, por loja, por vendedor, por método de pagamento
- Top 20 produtos mais vendidos
- Stock crítico (zerado e abaixo do mínimo)
- Reparações por estado

### Gestão Multi-loja (admin)
- Criar e gerir lojas ilimitadas
- Stock por loja (cada loja tem o seu stock)
- Transferência de stock entre lojas
- Utilizadores ligados a lojas específicas

### Fornecedores
- Base de dados de fornecedores
- Registo de preço de custo vs preço de venda
- Cálculo automático de margem

### Terminal Web
- Aceder ao sistema via browser: `http://IP:2021`
- Funciona com `gestao_loja` no terminal do browser

---

## Permissões

| Role      | Acesso |
|-----------|--------|
| admin     | Tudo: lojas, utilizadores, relatórios, dados |
| gerente   | Tudo exceto gestão de utilizadores/lojas; relatórios completos |
| vendedor  | Vendas, clientes, reparações, orçamentos, caixa |
| tecnico   | Reparações, garantias, consultar clientes |

**Regra de loja:** um utilizador com `loja_id` definido só consegue
autenticar nessa loja. Gerentes sem `loja_id` têm acesso global.

---

## Ficheiros de Dados

Todos em `data/` no servidor:

| Ficheiro            | Conteúdo                        |
|---------------------|---------------------------------|
| `utilizadores.json` | Utilizadores e passwords (SHA-256) |
| `lojas.json`        | Lojas registadas                |
| `clientes.json`     | Clientes (nunca apagados)       |
| `produtos.json`     | Catálogo com stock por loja     |
| `vendas.json`       | Faturas                         |
| `devolucoes.json`   | Notas de devolução              |
| `orcamentos.json`   | Orçamentos                      |
| `reparacoes.json`   | Fichas de reparação             |
| `garantias.json`    | Certificados de garantia        |
| `caixa.json`        | Movimentos de caixa             |
| `fornecedores.json` | Fornecedores                    |
| `fidelizacao.json`  | Pontos de clientes              |
| `logs.json`         | Auditoria de todas as ações     |

**Backup:** copiar a pasta `data/` é suficiente para fazer backup completo.

---

## Protocolo de Rede

Comunicação via TCP na porta 2022.
Cada mensagem é uma linha JSON terminada em `\n`.

```
Cliente → Servidor:  {"cmd":"VND_CRIAR","data":{...}}
Servidor → Cliente:  {"ok":true,"data":{...}}
                  ou {"ok":false,"erro":"mensagem de erro"}
```

Segurança: passwords enviadas já em hash SHA-256 (nunca em texto claro).
