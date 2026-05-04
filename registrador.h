#ifndef REGISTRADOR_H
#define REGISTRADOR_H

#include <sys/types.h>
#include "definiciones.h"

void hacer_registro(int p_lec, int p_esc, pid_t papa, ShmSistema *memoria);

#endif