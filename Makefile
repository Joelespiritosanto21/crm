# Makefile — Sistema de Gestão Eletrónica
# Uso: make        → compila
#      make clean  → limpa binários
#      make run    → compila e executa

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
TARGET   = gestao

SRCS = main.cpp auth.cpp clientes.cpp produtos.cpp vendas.cpp \
       orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
       logs.cpp documentos.cpp

OBJS = $(SRCS:.cpp=.o)

all: dirs $(TARGET)

dirs:
	@mkdir -p data docs

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo ""
	@echo "  [OK] Compilação concluída: ./$(TARGET)"
	@echo ""

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "  Limpeza concluída."

clean-data:
	@echo "  ATENÇÃO: Isto apaga todos os dados!"
	rm -rf data/ docs/
	mkdir -p data docs

.PHONY: all dirs run clean clean-data
