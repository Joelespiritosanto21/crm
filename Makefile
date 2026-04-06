# =============================================================
# TechFix — Makefile completo
# =============================================================
#
#  make                → compila tudo
#  make gestao_server  → servidor central
#  make gestao_loja    → cliente das lojas
#  make gestao         → terminal standalone (sem rede)
#  make runserver      → inicia o servidor
#  make clean          → limpa binários
# =============================================================

CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall \
           -Wno-unused-result -Wno-unused-variable \
           -Wno-unused-function -Wno-unused-parameter

SRCS_STANDALONE = main.cpp auth.cpp clientes.cpp produtos.cpp \
                  vendas.cpp orcamentos.cpp reparacoes.cpp \
                  garantias.cpp lojas.cpp logs.cpp documentos.cpp

all: dirs gestao_server gestao_loja gestao
	@echo ""
	@echo "  ✓ Compilação completa."
	@echo "  Servidor : ./gestao_server"
	@echo "  Lojas    : ./gestao_loja"
	@echo ""

dirs:
	@mkdir -p data docs

gestao_server: gestao_server.cpp server_handlers.h net_utils.h protocolo.h json_utils.h sha256.h
	$(CXX) $(CXXFLAGS) -o gestao_server gestao_server.cpp -lpthread -lutil
	@echo "  [OK] gestao_server"

gestao_loja: gestao_loja.cpp client.h net_utils.h protocolo.h common.h json_utils.h sha256.h
	$(CXX) $(CXXFLAGS) -o gestao_loja gestao_loja.cpp -lpthread
	@echo "  [OK] gestao_loja"

gestao: $(SRCS_STANDALONE)
	$(CXX) $(CXXFLAGS) -o gestao $(SRCS_STANDALONE)
	@echo "  [OK] gestao (standalone)"

runserver: gestao_server
	./gestao_server

clean:
	rm -f gestao_server gestao_loja gestao *.o
	@echo "  Limpeza concluída."

.PHONY: all dirs runserver clean
