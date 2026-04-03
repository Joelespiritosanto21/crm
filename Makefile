# Makefile — TechFix Sistema de Gestao
#
#  make           → compila gestao (terminal) e webserver
#  make gestao    → apenas terminal
#  make web       → apenas servidor web
#  make run       → terminal direto
#  make runweb    → servidor web (abrir http://localhost:2021)
#  make clean     → limpar binarios

CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function

SRCS_MAIN = main.cpp auth.cpp clientes.cpp produtos.cpp vendas.cpp \
            orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
            logs.cpp documentos.cpp

all: dirs gestao webserver

dirs:
	@mkdir -p data docs

gestao: $(SRCS_MAIN)
	$(CXX) $(CXXFLAGS) -o gestao $(SRCS_MAIN)
	@echo "  [OK] gestao compilado"

webserver: webserver.cpp
	$(CXX) $(CXXFLAGS) -o webserver webserver.cpp -lpthread
	@echo "  [OK] webserver compilado"

server_api: server_api.cpp
	$(CXX) $(CXXFLAGS) -o server_api server_api.cpp
	@echo "  [OK] server_api compilado"

run: gestao
	./gestao

runweb: gestao webserver
	@echo ""
	@echo "  Abrindo servidor web em http://localhost:2021 ..."
	@echo "  Ctrl+C para parar."
	@echo ""
	./webserver

clean:
	rm -f gestao webserver *.o
	@echo "  Limpeza concluida."

.PHONY: all dirs run runweb clean
