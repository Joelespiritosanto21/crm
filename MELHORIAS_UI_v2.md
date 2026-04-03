# GUIA DE MELHORIAS DE UI - CRM v2.0

## O Que Mudou

A interface do usuário foi completamente reformulada com:

### ✨ Visual Profissional

- **Cabeçalhos com data/hora integrada** em caixa decorativa
- **Títulos em diferentes níveis** (primário, simples, seção, subseção)
- **Cores ANSI** que funcionam em Windows, Linux e macOS
- **Tabelas formatadas** com bordas Unicode profissionais
- **Menus elegantes** com ícones visuais

### 🎨 Novos Elementos Visuais

- Cores para diferentes tipos de mensagens
- Ícones de status: ✓ (sucesso), ✗ (erro), ⚠ (aviso), ℹ (info)
- Caixas decorativas com múltiplos estilos
- Separadores visuais com caracteres Unicode
- Alinhamento profissional de conteúdo

### 📊 Tabelas Melhoradas

Agora as tabelas têm:

- Cabeçalhos destacados em CYAN BOLD
- Linhas alternadas em cores diferentes
- Bordas Unicode (┌ ─ ┐ │ ├ ┤ ┼ └)
- Cálculo automático de largura de colunas
- Espaçamento profissional

### 🎯 Menus Melhorados

- Ícones de seleção (➜ e ►)
- Números de opção em cores diferentes
- Estrutura clara entre submenu e menu principal
- Melhor espaçamento visual

### 📝 Validação de Entrada

Novos métodos para entrada de dados:

```cpp
std::string texto = g_ui.lerTexto("Nome do cliente:");
int opcao = g_ui.lerInteiro("Escolha uma opção:");
double preco = g_ui.lerDouble("Preço do produto:");
```

### 📢 Mensagens de Status

```cpp
g_ui.sucesso("Produto criado com sucesso!");
g_ui.erro("Produto não encontrado.");
g_ui.aviso("Aviso: stock baixo!");
g_ui.info("Servidor iniciado na porta 2021");
```

## Exemplo de Uso

### Antes (UI Antiga)

```
====================================================
GESTÃO DE PRODUTOS
====================================================

[1] Criar produto/serviço
[2] Listar catálogo
```

### Depois (Nova UI)

```
  ══════════════════════════════════════════════════════════════
  ║  01/04/2026 14:32:47                                       ║
  ══════════════════════════════════════════════════════════════

  ▓▓▓▓▓▓▓▓▓▓▓ GESTÃO DE PRODUTOS ▓▓▓▓▓▓▓▓▓▓▓

     [1]  Criar produto/serviço
     [2]  Listar catálogo
     [3]  Pesquisar por EAN
     [4]  Editar produto
     [5]  Desativar produto
     [6]  Alertas de stock

     [0]  Sair

  ➜ Opção: █
```

## Novas Funções Disponíveis

### Títulos

- `tituloprincipal(titulo)` - Título principal com decoração
- `titulosimples(titulo)` - Título simples
- `secao(titulo)` - Início de seção
- `subsecao(titulo)` - Subseção

### Tabelas

- `mostrarTabela(cabecalhos, dados)` - Tabela com bordas Unicode

### Caixas

- `caixaDestaque(titulo, conteudo)` - Caixa de destaque com título
- `caixaSimples(conteudo)` - Caixa simples

### Mensagens

- `sucesso(mensagem)` - Mensagem de sucesso em verde
- `erro(mensagem)` - Mensagem de erro em vermelho
- `aviso(mensagem)` - Mensagem de aviso em amarelo
- `info(mensagem)` - Mensagem de informação em cyan

### Entrada

- `lerTexto(prompt)` - Entrada de texto
- `lerInteiro(prompt)` - Entrada de inteiro com validação
- `lerDouble(prompt)` - Entrada de número decimal com validação

### Utilitários

- `pausar(msg)` - Pausar com mensagem customizada
- `delay(ms)` - Aguardar N milissegundos

## Paleta de Cores

```cpp
// Cores disponíveis (via macros):
RESET, BOLD, DIM, ITALIC, UNDERLINE
BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
BG_BLACK, BG_RED, BG_GREEN, BG_YELLOW, BG_BLUE
```

## Compatibilidade

- ✅ **Windows 10/11** com terminal padrão (cmd.exe, PowerShell, Windows Terminal)
- ✅ **Linux** (Ubuntu, Debian, CentOS, etc.)
- ✅ **macOS** (Terminal.app, iTerm)
- ✅ **WSL2** (Windows Subsystem for Linux)

## Exemplo Completo

```cpp
#include "ui.h"

int main() {
    g_ui.inicializar();

    // Exibir título principal
    g_ui.tituloprincipal("SISTEMA DE GESTÃO");

    // Criar menu
    std::vector<std::string> opcoes = {
        "Gestão de Clientes",
        "Gestão de Produtos",
        "Vendas e Faturas",
        "Reparações"
    };

    g_ui.mostrarMenu("MENU PRINCIPAL", opcoes);
    int escolha = g_ui.lerInteiro();

    if (escolha == 1) {
        g_ui.secao("GESTÃO DE CLIENTES");
        g_ui.sucesso("Cliente criado com sucesso!");

        // Exibir tabela
        std::vector<std::string> cabeca = {"ID", "Nome", "Email"};
        std::vector<std::vector<std::string>> dados = {
            {"1", "João Silva", "joao@email.pt"},
            {"2", "Maria Santos", "maria@email.pt"}
        };
        g_ui.mostrarTabela(cabeca, dados);
    }

    g_ui.pausar();
    return 0;
}
```

## Próximos Passos

1. As novas funções estão prontas em `ui.h` e `ui.cpp`
2. Compile com: `make clean && make`
3. A UI será automaticamente aplicada em todos os menus
4. Pode customizar cores e estilos conforme necessário

## Troubleshooting

**P: As cores não aparecem no terminal?**
R: Algumas versões antigas do Windows cmd.exe (legacy) não suportam ANSI colors. Use Windows Terminal (grátis da Microsoft Store) para melhor compatibilidade.

**P: Como desativar as cores?**
R: No topo de `ui.h`, comente todos os defines de cor:

```cpp
// #define GREEN   "\033[32m"
```

**P: Posso usar caracteres Unicode em Windows?**
R: Sim! Windows 10+ suporta Unicode nativamente no terminal.
