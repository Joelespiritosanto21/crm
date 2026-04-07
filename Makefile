# =============================================================
# TechFix — Makefile (Linux/macOS)
#
#  make                → compila tudo
#  make gestao_server  → servidor central
#  make gestao_loja    → cliente das lojas (Linux)
#  make runserver      → inicia o servidor
#  make clean          → limpa binários
#
# Windows: usar compilar_windows.bat
# =============================================================

CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall \
           -Wno-unused-result -Wno-unused-variable \
           -Wno-unused-function -Wno-unused-parameter

all: dirs gestao_server gestao_loja
	@echo ""
	@echo "  Compilacao completa."
	@echo "  Servidor : ./gestao_server"
	@echo "  Cliente  : ./gestao_loja"
	@echo ""

dirs:
	@mkdir -p data docs

gestao_server: gestao_server.cpp server_handlers.h net_utils.h protocolo.h json_utils.h sha256.h
	$(CXX) $(CXXFLAGS) -o gestao_server gestao_server.cpp -lpthread -lutil
	@echo "  [OK] gestao_server"

gestao_loja: gestao_loja.cpp client.h net_utils.h protocolo.h common.h json_utils.h sha256.h
	$(CXX) $(CXXFLAGS) -o gestao_loja gestao_loja.cpp -lpthread
	@echo "  [OK] gestao_loja"

runserver: gestao_server
	./gestao_server

clean:
	rm -f gestao_server gestao_loja *.o
	@echo "  Limpeza concluida."

.PHONY: all dirs runserver clean
