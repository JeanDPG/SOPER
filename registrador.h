#ifndef REGISTRADOR_H
#define REGISTRADOR_H

#include <sys/types.h>

void ejecutar_registrador(int pipe_lectura, int pipe_escritura, pid_t ppid);

#endif
