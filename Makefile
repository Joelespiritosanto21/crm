# Makefile — Sistema de Gestão Eletrónica
# Uso: make        → compila
#      make clean  → limpa binários
#      make run    → compila e executa
#
# Compatível com Windows (MinGW/MSVC) e Linux/macOS

# Detectar SO
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CXX      = g++
    CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -pthread
    LDFLAGS  = -lstdc++ -lm
    TARGET   = gestao
    CLEAN_CMD = rm -f
    EXE_EXT  = 
endif
ifeq ($(UNAME_S),Darwin)
    CXX      = clang++
    CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -pthread
    LDFLAGS  = -lstdc++ -lm
    TARGET   = gestao
    CLEAN_CMD = rm -f
    EXE_EXT  = 
endif
ifeq ($(OS),Windows_NT)
    CXX      = g++
    CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -pthread -D_WIN32_WINNT=0x0601
    LDFLAGS  = -lws2_32
    TARGET   = gestao.exe
    CLEAN_CMD = del /Q
    EXE_EXT  = .exe
endif

# Valores padrão para SO desconhecido
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -pthread
LDFLAGS  ?= -lstdc++ -lm
TARGET   ?= gestao
CLEAN_CMD ?= rm -f

SRCS = main.cpp auth.cpp clientes.cpp produtos.cpp vendas.cpp \
       orcamentos.cpp reparacoes.cpp garantias.cpp lojas.cpp \
       logs.cpp documentos.cpp ui.cpp server.cpp

OBJS = $(SRCS:.cpp=.o)

all: dirs $(TARGET)

dirs:
	@mkdir -p data docs

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  [OK] Compilação concluída: ./$(TARGET)"
	@echo ""
	@echo "  Interface CLI: execute ./$(TARGET)"
	@echo "  Interface Web: http://localhost:2021 (após iniciar)"
	@echo ""

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	$(CLEAN_CMD) $(OBJS) $(TARGET)
	@echo "  Limpeza concluída."

clean-data:
	@echo "  ATENÇÃO: Isto apaga todos os dados!"
	rm -rf data/ docs/
	mkdir -p data docs

.PHONY: all dirs run clean clean-data

