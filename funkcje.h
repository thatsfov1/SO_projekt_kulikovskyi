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
void send_acknowledgment_to_kierownik();
void initialize_shm_sklep(int *shm_id, Sklep **sklep, int key);
void init_semaphores(int *sem_id, int key, int num_semaphores);
void init_semaphore_values(int sem_id, int num_semaphores);
void initialize_message_queue(int *msqid, int key);
void init_kosz(Kosz *kosz, Sklep *sklep);
void drukuj_produkt(const char* nazwa, int ilosc);
int znajdz_kase_z_najmniejsza_kolejka(Sklep *sklep, int sem_id);
void losuj_liste_zakupow(Sklep *sklep, Produkt lista_zakupow[], int *liczba_produktow);
void setup_signal_handlers(void (*cleanup_handler)(int), void (*evacuation_handler)(int));
#endif