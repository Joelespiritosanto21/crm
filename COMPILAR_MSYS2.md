# Compilar no Windows com MSYS2 — Guia Rápido

## O problema

O MSYS2 tem **vários terminais diferentes**. O GCC instalado com
`pacman -S mingw-w64-ucrt-x86_64-gcc` só está disponível no terminal
**UCRT64** — não no terminal **MSYS** (o genérico azul escuro).

## Solução: usar o terminal correcto

### Passo 1 — Abrir o terminal certo

No menu Iniciar do Windows, procurar **MSYS2** e escolher:

```
MSYS2 UCRT64        ← usar este  (fundo preto com "UCRT64" no título)
```

**NÃO usar:**
- `MSYS2 MSYS` — terminal genérico, sem compiladores nativos Windows
- `MSYS2 MINGW32` — 32 bits (não queremos)

### Passo 2 — Instalar GCC (se não tiver)

No terminal **UCRT64**:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
```

### Passo 3 — Navegar para a pasta do projecto

```bash
cd /d/CRM/projeto
# ou
cd "D:/CRM/projeto"
```

### Passo 4 — Compilar

```bash
./compilar.sh
```

Se o script não funcionar, compilar directamente:
```bash
g++ -std=c++11 -O2 -o gestao_loja.exe gestao_loja.cpp -lws2_32
```

### Passo 5 — Configurar e executar

```bash
# Editar servidor.conf com o IP do servidor
nano servidor.conf
# ou
notepad servidor.conf

# Executar
./gestao_loja.exe
```

---

## Verificar que está no terminal correcto

No prompt do MSYS2, o terminal correcto mostra:

```
UCRT64 /d/CRM/projeto $        ← correcto (mostra UCRT64)
MINGW64 /d/CRM/projeto $       ← também funciona
MSYS /d/CRM/projeto $          ← ERRADO (não tem g++ Windows)
```

Também podes verificar:
```bash
which g++
# deve mostrar: /ucrt64/bin/g++  ou  /mingw64/bin/g++
# NÃO deve mostrar: /usr/bin/g++  (esse é o gcc do MSYS, gera binários que precisam de DLLs)
```

---

## Alternativa: compilar pelo CMD do Windows

Se tiveres o PATH configurado com `C:\msys64\ucrt64\bin`:

```bat
cd D:\CRM\projeto
compilar_windows.bat
```

Para adicionar ao PATH:
1. Win+Pause → Configurações avançadas → Variáveis de ambiente
2. Em "Variáveis do sistema" → Path → Editar → Novo
3. Adicionar: `C:\msys64\ucrt64\bin`
4. OK em tudo → abrir novo CMD → tentar `g++ --version`
