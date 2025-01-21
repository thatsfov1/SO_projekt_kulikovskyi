#ifndef FUNKCJE_H
#define FUNKCJE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include "struktury.h"

// Deklaracje funkcji
void sem_wait(int sem_id, int sem_num);
void sem_post(int sem_id, int sem_num);
void init_produkty(Sklep *sklep);
void init_kosz(Kosz *kosz, Sklep *sklep);

void setup_signal_handlers(void (*cleanup_handler)(int), void (*evacuation_handler)(int));
#endif