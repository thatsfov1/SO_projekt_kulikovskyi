#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include "struktury.h"
#include "funkcje.h"

Sklep *sklep;
int sem_id;
int sklep_zamkniety = 0;
int kasa_id;
int semop_wait_invalid_argument = 0;

void cleanup_handler(int signum) {
    exit(0);
}

void evacuation_handler(int signum) {
    printf("Kasa %d: Ewakuacja!, zamykam się.\n", kasa_id + 1);
    cleanup_handler(SIGUSR1);
}

void monitoruj_kasy(Sklep *sklep, int sem_id) {

    while (!sklep->sklep_zamkniety) {
        sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
        int ilosc_klientow = sklep->ilosc_klientow;
        sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);

        sem_wait(sem_id, SEM_CASHIER_MUTEX + 2);
        if (ilosc_klientow < 10) {
            if (sklep->kasjerzy[2].ilosc_klientow != -1) {
                sklep->kasjerzy[2].ilosc_klientow = -1;
                printf("Kasa 3: Zamknięto, mniej niż 10 klientów w sklepie.\n");
            }
        } else {
            if (sklep->kasjerzy[2].ilosc_klientow == -1) {
                sklep->kasjerzy[2].ilosc_klientow = 0;
                printf("Kasa 3: Otwarto, więcej niż 10 klientów w sklepie.\n");
            }
        }
        sem_post(sem_id, SEM_CASHIER_MUTEX + 2);

        sleep(1);
    }
}

int pobierz_id_klienta_z_kolejki(Sklep *sklep, int kasa_id) {
    sem_wait(sem_id, SEM_QUEUE_MUTEX);
    
    if (sklep->kasjerzy[kasa_id].head == sklep->kasjerzy[kasa_id].tail) {
        sem_post(sem_id, SEM_QUEUE_MUTEX);
        return -1;
    }
    
    int klient_index = sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].head];
    

    sklep->kasjerzy[kasa_id].head = (sklep->kasjerzy[kasa_id].head + 1) % MAX_KLIENTOW;
    sklep->kasjerzy[kasa_id].ilosc_klientow--;
    
    sem_post(sem_id, SEM_QUEUE_MUTEX);
    return klient_index;
}

void obsluz_klienta(Sklep *sklep, int kasa_id, int sem_id) {
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid;
    initialize_message_queue(&msqid, key);
    message_buf rbuf;

    while (1) {

        sem_wait(sem_id, SEM_MUTEX_STORE);
        int zamkniety = sklep->sklep_zamkniety;
        sem_post(sem_id, SEM_MUTEX_STORE);

        sem_wait(sem_id, SEM_CASHIER_MUTEX + kasa_id);
        if (sklep->kasjerzy[kasa_id].ilosc_klientow > 0) {
            int klient_index = pobierz_id_klienta_z_kolejki(sklep, kasa_id);
            sem_post(sem_id, SEM_CASHIER_MUTEX + kasa_id);

            if (klient_index != -1) {
                sem_wait(sem_id, SEM_MUTEX_CUSTOMERS);
                if (sklep->klienci[klient_index].klient_id != 0) {
                float suma = 0;
                for (int i = 0; i < sklep->klienci[klient_index].ilosc_zakupow; i++) {
                    Produkt *produkt = &sklep->klienci[klient_index].lista_zakupow[i];
                    if (produkt->ilosc > 0) {
                        suma += produkt->ilosc * produkt->cena;
                        sklep->kasjerzy[kasa_id].ilosc_sprzedanych[produkt->id] += produkt->ilosc;
                    }
                }

                sleep(3);  // Symulacja obsługi klienta
                
                message_buf sbuf;
                sbuf.mtype = sklep->klienci[klient_index].klient_id;
                strcpy(sbuf.mtext, klient_rozliczony);
                msgsnd(msqid, &sbuf, sizeof(sbuf.mtext), 0);

                printf("Kasa %d: Obsłużono klienta %d, suma zakupów: %.2f zł\n",
                       kasa_id + 1, sklep->klienci[klient_index].klient_id, suma);

                
                sklep->klienci[klient_index].klient_id = 0;
                
            }
            sem_post(sem_id, SEM_MUTEX_CUSTOMERS);
            }
        } else {
            sem_post(sem_id, SEM_CASHIER_MUTEX + kasa_id);
            if (zamkniety) {
                printf("Kasa %d: Zamykam się, brak klientów w kolejce.\n", kasa_id + 1);
                break;
            }
            usleep(100000);
        }
    }

    sem_wait(sem_id, SEM_QUEUE_MUTEX);
    sklep->kasjerzy[kasa_id].ilosc_klientow = 0;
    sklep->kasjerzy[kasa_id].head = sklep->kasjerzy[kasa_id].tail = 0;
    sem_post(sem_id, SEM_QUEUE_MUTEX);

    send_acknowledgment_to_kierownik();
}

int main() {
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    chld_handler();

    int shm_id;
    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    for (int i = 0; i < MAX_KASJEROW; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGUSR1, evacuation_handler);
            kasa_id = i;
            obsluz_klienta(sklep, i, sem_id);
            exit(0);
        }
    }

    monitoruj_kasy(sklep, sem_id);
    return 0;
}
