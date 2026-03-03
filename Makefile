# Nombre del ejecutable final
TARGET = mrush

# Compilador
CC = gcc

# Banderas de compilación (Warnings, Debug, Pthreads)
CFLAGS = -Wall -Wextra -g -pthread

# Bibliotecas adicionales (si pow.h necesita la librería matemática, añade -lm)
LIBS = -pthread

# Archivos fuente (.c)
SRCS = main.c minero.c registrador.c pow.c

# Archivos objeto (.o) - Se generan automáticamente a partir de SRCS
OBJS = $(SRCS:.c=.o)

# Archivos de cabecera (.h) - Para detectar cambios y recompilar
DEPS = definiciones.h minero.h registrador.h pow.h

# Regla principal: compilar todo
all: $(TARGET)

# Cómo crear el ejecutable uniendo los archivos objeto
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Cómo compilar cada archivo .c en un .o
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para limpiar archivos temporales y el ejecutable
clean:
	rm -f $(OBJS) $(TARGET) miner_*.txt

# Regla para ejecutar con un ejemplo rápido (opcional)
run: all
	./$(TARGET) 0 5 4

.PHONY: all clean run
