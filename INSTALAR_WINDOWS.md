# Instalar TechFix no Windows — Guia Completo

## O que é necessário

- Windows 10 ou 11 (64 bits)
- Compilador g++ (MinGW) — instruções abaixo
- Ligação ao servidor TechFix (o servidor corre em Linux/VPS)

---

## Passo 1 — Instalar o compilador g++ (MinGW)

### Opção A: WinLibs (mais simples, sem instalador)

1. Ir a **https://winlibs.com/**
2. Clicar em **"Download"** → secção **UCRT runtime** → **Win64** → escolher o `.zip` mais recente (ex: `winlibs-x86_64-...-gcc-14.x.x-...zip`)
3. Extrair o ZIP para `C:\mingw64`
4. Adicionar `C:\mingw64\bin` ao PATH:
   - Tecla Windows → procurar **"variáveis de ambiente"**
   - **Editar as variáveis de ambiente do sistema**
   - Variáveis de sistema → **Path** → **Editar** → **Novo**
   - Escrever `C:\mingw64\bin`
   - OK em tudo
5. Abrir novo CMD e testar: `g++ --version`

### Opção B: MSYS2 (mais completo)

1. Ir a **https://www.msys2.org/** e instalar
2. Abrir **MSYS2 MinGW 64-bit** (no menu Iniciar)
3. Correr: `pacman -S mingw-w64-x86_64-gcc`
4. Usar sempre o terminal **MSYS2 MinGW 64-bit** para compilar e executar

---

## Passo 2 — Compilar o cliente

Abrir **CMD** ou **PowerShell** na pasta do projecto e correr:

```bat
compilar_windows.bat
```

Ou manualmente:
```bat
g++ -std=c++11 -O2 -o gestao_loja.exe gestao_loja.cpp -lws2_32
```

Após compilar, o ficheiro `gestao_loja.exe` é criado na mesma pasta.

---

## Passo 3 — Configurar o servidor

Criar um ficheiro `servidor.conf` na mesma pasta que `gestao_loja.exe`:

```ini
host=45.131.64.57
port=2022
loja_id=loj_xxxxxxxxxxxxxxx
```

Substituir:
- `45.131.64.57` pelo IP do teu servidor
- `loj_xxx...` pelo ID da loja (obtido no menu Administração → Lojas)

Na **primeira execução** sem `servidor.conf`, o programa pergunta automaticamente.

---

## Passo 4 — Executar

Fazer duplo clique em `gestao_loja.exe`, ou no CMD:

```bat
gestao_loja.exe
```

### Para melhores cores e caracteres

Usar o **Windows Terminal** (disponível na Microsoft Store gratuitamente) em vez do CMD clássico. Suporta cores ANSI e caracteres UTF-8 correctamente.

Se usar CMD clássico, correr antes:
```bat
chcp 65001
```

---

## Instalação automática (opcional)

O script `instalar_loja_windows.bat`:
- Cria a pasta `%USERPROFILE%\TechFix`
- Copia o executável
- Configura o `servidor.conf`
- Cria atalho no Ambiente de Trabalho

```bat
instalar_loja_windows.bat
```

---

## Estrutura de ficheiros no PC da loja

```
pasta_da_loja\
├── gestao_loja.exe       ← executável principal
├── servidor.conf         ← configuração do servidor
├── data\                 ← criado automaticamente (não usado no cliente)
└── docs\                 ← documentos gerados localmente
```

---

## Problemas comuns

### "g++ não encontrado"
→ MinGW não está no PATH. Ver Passo 1.

### "Nao foi possivel ligar ao servidor"
→ Verificar:
1. O servidor `gestao_server` está a correr no VPS?
2. O IP em `servidor.conf` está correcto?
3. A porta 2022 está aberta na firewall do servidor? (`sudo ./firewall.sh` no servidor)
4. Há ligação à internet?

### Caracteres estranhos no ecrã (□ □ □)
→ Usar **Windows Terminal** ou correr `chcp 65001` no CMD antes de abrir o programa.

### Cores não aparecem
→ O CMD clássico do Windows 7/8 não suporta ANSI. Actualizar para Windows 10/11 e usar Windows Terminal.

### "Acesso negado" ao executar
→ Clicar com o botão direito em `gestao_loja.exe` → Propriedades → Desbloquear.

---

## Atalho rápido no Ambiente de Trabalho

1. Clicar com botão direito no Ambiente de Trabalho → Novo → Atalho
2. Localização: `C:\caminho\para\gestao_loja.exe`
3. Nome: `TechFix`
4. Clicar com botão direito no atalho → Propriedades
5. Em "Iniciar em" colocar a pasta onde está o `servidor.conf`
