# Compilador y banderas
CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread
LIBS    = -pthread -lrt

# Cabeceras comunes
DEPS = definiciones.h pow.h

# ---- Ejecutable mrush (proceso minero) ----
MRUSH       = mrush
MRUSH_SRCS  = main.c minero.c registrador.c pow.c
MRUSH_OBJS  = $(MRUSH_SRCS:.c=.o)
MRUSH_DEPS  = $(DEPS) minero.h registrador.h

# ---- Ejecutable monitor (Comprobador + Monitor) ----
MONITOR      = monitor
MONITOR_SRCS = monitor.c pow.c
MONITOR_OBJS = $(MONITOR_SRCS:.c=.o)
MONITOR_DEPS = $(DEPS)

# Regla principal: compilar ambos
all: $(MRUSH) $(MONITOR)

# ---- Reglas de enlazado ----
$(MRUSH): $(MRUSH_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MRUSH_OBJS) $(LIBS)

$(MONITOR): $(MONITOR_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MONITOR_OBJS) $(LIBS)

# ---- Reglas de compilación ----
main.o:        main.c        $(MRUSH_DEPS)
	$(CC) $(CFLAGS) -c main.c -o main.o
minero.o:      minero.c      $(MRUSH_DEPS)
	$(CC) $(CFLAGS) -c minero.c -o minero.o
registrador.o: registrador.c $(MRUSH_DEPS)
	$(CC) $(CFLAGS) -c registrador.c -o registrador.o
monitor.o:     monitor.c     $(MONITOR_DEPS)
	$(CC) $(CFLAGS) -c monitor.c -o monitor.o
pow.o:         pow.c         pow.h
	$(CC) $(CFLAGS) -c pow.c -o pow.o

# ---- Limpieza ----
clean:
	rm -f *.o $(MRUSH) $(MONITOR) [0-9]*.txt

.PHONY: all clean