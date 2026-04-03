/*
 * auth.cpp - Autenticacao e gestao de utilizadores
 */

#include "auth.h"
#include "sha256.h"
#include "logs.h"
#include <iostream>
#include <iomanip>

/* Leitura de password cross-platform (sem eco) */
static std::string lerPassword(const std::string& prompt) {
    std::cout << "  " << prompt;
    std::cout.flush();
    std::string pw;
#ifdef _WIN32
    #include <conio.h>
    char c;
    while ((c = _getch()) != '\r' && c != '\n') {
        if (c == '\b') { if (!pw.empty()) { pw.pop_back(); std::cout << "\b \b"; } }
        else           { pw += c; std::cout << '*'; }
    }
    std::cout << "\n";
#else
    /* Linux/macOS: desativar eco via escape ANSI (sem termios) */
    std::cout << "\033[8m";   /* esconder texto */
    std::getline(std::cin, pw);
    std::cout << "\033[28m";  /* mostrar texto */
    std::cout << "\n";
#endif
    return pw;
}

/* Validar username: exatamente 3 letras */
static bool validarUsername(const std::string& u) {
    if (u.size() != 3) return false;
    for (char c : u) if (!std::isalpha(c)) return false;
    return true;
}

/* Inicializar: criar admin padrão se não existir */
void authInicializar() {
    JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
    if (!utils.isArray()) utils = JsonValue(JsonArray{});

    if (utils.arr.empty()) {
        // Criar admin padrão: username "adm", password "admin123"
        JsonObject admin;
        admin["id"]       = JsonValue(generateId("usr_"));
        admin["username"] = JsonValue(std::string("adm"));
        admin["password"] = JsonValue(hashPassword("admin123"));
        admin["nome"]     = JsonValue(std::string("Administrador"));
        admin["role"]     = JsonValue(std::string("admin"));
        admin["estado"]   = JsonValue(std::string("ativo"));
        admin["loja_id"]  = JsonValue(std::string(""));
        admin["criado_em"]= JsonValue(dataAtual());

        utils.arr.push_back(JsonValue(admin));
        jsonSaveFile(FILE_UTILIZADORES, utils);

        std::cout << "\n  [!] Admin padrão criado: username='adm' password='admin123'\n";
        std::cout << "      Altere a password após o primeiro login!\n\n";
    }
}

/* Login */
bool authLogin() {
    titulo("LOGIN DO SISTEMA");

    for (int tentativas=0; tentativas<3; ++tentativas) {
        std::string user = lerString("  Username (3 letras): ");
        if (!validarUsername(user)) {
            std::cout << "  [!] Username inválido (exatamente 3 letras).\n";
            continue;
        }

        std::string pw = lerPassword("  Password: ");
        std::string hash = hashPassword(pw);

        JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
        if (!utils.isArray()) continue;

        for (auto& u : utils.arr) {
            if (u["username"].asString() == user && u["password"].asString() == hash) {
                if (u["estado"].asString() != "ativo") {
                    std::cout << "  [!] Conta suspensa. Contacte o administrador.\n";
                    return false;
                }
                g_sessao.username    = user;
                g_sessao.role        = u["role"].asString();
                g_sessao.loja_id     = u["loja_id"].asString();
                g_sessao.autenticado = true;

                logRegistar("login", "utilizador=" + user);
                std::cout << "\n  Bem-vindo, " << u["nome"].asString()
                          << " [" << g_sessao.role << "]\n";
                return true;
            }
        }
        std::cout << "  [!] Credenciais inválidas. (" << (2-tentativas) << " tentativas restantes)\n";
    }
    return false;
}

/* Reautenticação para ações críticas */
bool authReautenticar() {
    std::cout << "\n  [!] Ação crítica. Confirme a sua identidade.\n";
    std::string pw = lerPassword("  Password: ");
    std::string hash = hashPassword(pw);

    JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
    if (!utils.isArray()) return false;

    for (auto& u : utils.arr) {
        if (u["username"].asString() == g_sessao.username && u["password"].asString() == hash)
            return true;
    }
    std::cout << "  [!] Password incorreta.\n";
    return false;
}

/* Logout */
void authLogout() {
    logRegistar("logout", "utilizador=" + g_sessao.username);
    g_sessao = Sessao{};
    std::cout << "  Sessão terminada.\n";
}

/* Criar utilizador */
void utilizadoresCriar() {
    if (!temPermissao("admin")) { erroPermissao(); return; }

    subtitulo("CRIAR UTILIZADOR");

    std::string user;
    while (true) {
        user = lerString("  Username (3 letras): ");
        if (!validarUsername(user)) { std::cout << "  [!] Deve ter exatamente 3 letras.\n"; continue; }

        // Verificar duplicado
        JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
        bool dup = false;
        if (utils.isArray()) for (auto& u : utils.arr) if (u["username"].asString()==user) { dup=true; break; }
        if (dup) { std::cout << "  [!] Username já existe.\n"; continue; }
        break;
    }

    std::string nome = lerString("  Nome completo: ");
    while (nome.empty()) { std::cout << "  Nome obrigatório.\n"; nome = lerString("  Nome: "); }

    std::cout << "  Roles: admin | gerente | vendedor | tecnico\n";
    std::string role;
    while (true) {
        role = lerString("  Role: ");
        if (role=="admin"||role=="gerente"||role=="vendedor"||role=="tecnico") break;
        std::cout << "  Role inválido.\n";
    }

    std::string pw = lerPassword("  Password: ");
    std::string pw2 = lerPassword("  Confirmar password: ");
    if (pw != pw2) { std::cout << "  [!] Passwords não coincidem.\n"; return; }
    if (pw.size() < 6) { std::cout << "  [!] Password mínimo 6 caracteres.\n"; return; }

    JsonObject novo;
    novo["id"]       = JsonValue(generateId("usr_"));
    novo["username"] = JsonValue(user);
    novo["password"] = JsonValue(hashPassword(pw));
    novo["nome"]     = JsonValue(nome);
    novo["role"]     = JsonValue(role);
    novo["estado"]   = JsonValue(std::string("ativo"));
    novo["loja_id"]  = JsonValue(std::string(""));
    novo["criado_em"]= JsonValue(dataAtual());

    JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
    if (!utils.isArray()) utils = JsonValue(JsonArray{});
    utils.arr.push_back(JsonValue(novo));
    jsonSaveFile(FILE_UTILIZADORES, utils);

    logRegistar("criar_utilizador", "username=" + user + " role=" + role);
    std::cout << "\n  [OK] Utilizador '" << user << "' criado com sucesso.\n";
}

/* Listar utilizadores */
void utilizadoresListar() {
    if (!temPermissao("gerente")) { erroPermissao(); return; }

    JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
    subtitulo("LISTA DE UTILIZADORES");

    if (!utils.isArray() || utils.arr.empty()) {
        std::cout << "  Sem utilizadores.\n"; return;
    }

    std::cout << std::left
              << std::setw(6)  << "USER"
              << std::setw(25) << "NOME"
              << std::setw(12) << "ROLE"
              << std::setw(10) << "ESTADO" << "\n";
    linha();
    for (auto& u : utils.arr) {
        std::cout << std::setw(6)  << u["username"].asString()
                  << std::setw(25) << u["nome"].asString()
                  << std::setw(12) << u["role"].asString()
                  << std::setw(10) << u["estado"].asString() << "\n";
    }
}

/* Editar utilizador */
void utilizadoresEditar() {
    if (!temPermissao("admin")) { erroPermissao(); return; }

    subtitulo("EDITAR UTILIZADOR");
    utilizadoresListar();

    std::string user = lerString("\n  Username a editar: ");
    JsonValue utils = jsonParseFile(FILE_UTILIZADORES);
    if (!utils.isArray()) { std::cout << "  Sem dados.\n"; return; }

    for (auto& u : utils.arr) {
        if (u["username"].asString() == user) {
            std::cout << "  1. Alterar role atual: " << u["role"].asString() << "\n";
            std::cout << "  2. Alterar estado atual: " << u["estado"].asString() << "\n";
            std::cout << "  3. Redefinir password\n";
            std::cout << "  0. Cancelar\n";
            int op = lerInteiro("  Opção: ", 0, 3);

            if (op==1) {
                std::cout << "  Roles: admin | gerente | vendedor | tecnico\n";
                std::string r = lerString("  Novo role: ");
                if (r=="admin"||r=="gerente"||r=="vendedor"||r=="tecnico") {
                    u["role"] = JsonValue(r);
                    jsonSaveFile(FILE_UTILIZADORES, utils);
                    logRegistar("editar_utilizador", "username=" + user + " role=" + r);
                    std::cout << "  [OK] Role alterado.\n";
                }
            } else if (op==2) {
                std::string est = lerString("  Estado (ativo/suspenso): ");
                if (est=="ativo"||est=="suspenso") {
                    u["estado"] = JsonValue(est);
                    jsonSaveFile(FILE_UTILIZADORES, utils);
                    logRegistar("editar_utilizador", "username=" + user + " estado=" + est);
                    std::cout << "  [OK] Estado alterado.\n";
                }
            } else if (op==3) {
                if (!authReautenticar()) return;
                std::string pw  = lerPassword("  Nova password: ");
                std::string pw2 = lerPassword("  Confirmar: ");
                if (pw==pw2 && pw.size()>=6) {
                    u["password"] = JsonValue(hashPassword(pw));
                    jsonSaveFile(FILE_UTILIZADORES, utils);
                    std::cout << "  [OK] Password alterada.\n";
                } else {
                    std::cout << "  [!] Password inválida ou não coincide.\n";
                }
            }
            return;
        }
    }
    std::cout << "  [!] Utilizador não encontrado.\n";
}

/* Menu de utilizadores */
void utilizadoresMenu() {
    while (true) {
        titulo("GESTÃO DE UTILIZADORES");
        std::cout << "  1. Criar utilizador\n";
        std::cout << "  2. Listar utilizadores\n";
        std::cout << "  3. Editar utilizador\n";
        std::cout << "  0. Voltar\n";
        int op = lerInteiro("  Opção: ", 0, 3);
        if (op==0) break;
        else if (op==1) utilizadoresCriar();
        else if (op==2) utilizadoresListar();
        else if (op==3) utilizadoresEditar();
        pausar();
    }
}
