
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g
LDFLAGS = -lpthread

# Parámetros por defecto para ejecución
TARGET ?= 0
ROUNDS ?= 5
THREADS ?= 4
PID ?= 12345  # Para ver un log específico con make show_log

# Ejecutables
TARGETS = miner_a miner_b miner_c


all: $(TARGETS)

miner_a: miner_a.c pow.c
	$(CC) $(CFLAGS) miner_a.c pow.c -o miner_a $(LDFLAGS)

miner_b: miner_b.c pow.c
	$(CC) $(CFLAGS) miner_b.c pow.c -o miner_b $(LDFLAGS)

miner_c: miner_c.c pow.c
	$(CC) $(CFLAGS) miner_c.c pow.c -o miner_c $(LDFLAGS)

# Ejecucion

run_a: miner_a
	./miner_a $(TARGET) $(ROUNDS) $(THREADS)

run_b: miner_b
	./miner_b $(TARGET) $(ROUNDS) $(THREADS)

run_c: miner_c
	./miner_c $(TARGET) $(ROUNDS) $(THREADS)
# Valgirnd

valgrind_a: miner_a
	@echo "Apartado a"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		./miner_a $(TARGET) $(ROUNDS) $(THREADS)

valgrind_b: miner_b
	@echo "Apartado b"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		./miner_b $(TARGET) $(ROUNDS) $(THREADS)

valgrind_c: miner_c
	@echo "Apartado c"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		./miner_c $(TARGET) $(ROUNDS) $(THREADS)

# 

# Muestra el fichero de log
show_log:
	@echo "=== Contenido de $(PID).log ==="
	@if [ -f $(PID).log ]; then \
		cat $(PID).log; \
	else \
		echo "No existe el fichero $(PID).log"; \
		ls -la *.log 2>/dev/null || echo "No hay ficheros .log"; \
	fi

# Limpia archivos log
clean_logs:
	rm -f *.log

# Limpiar archivos compilados
clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean run_a run_b run_c valgrind_a valgrind_b valgrind_c show_log clean_logs