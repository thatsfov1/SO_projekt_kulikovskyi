#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include "struktury.h"
#include <string.h>
#include "funkcje.h"

Sklep *sklep;

void cleanup_handler(int signum) {
    shmdt(sklep);
    exit(0);
}
void evacuation_handler(int signum) {
    printf("Piekarz: Otrzymałem sygnał ewakuacji, kończę pracę.\n");
    cleanup_handler(signum);
}


void wypiekaj_produkty(Sklep *sklep, int sem_id) {
    srand(time(NULL));
    message_buf rbuf;

    key_t key = ftok("/tmp", msq_kierownik);
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }

    while (1) {
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, IPC_NOWAIT) != -1) {
            if (strcmp(rbuf.mtext, close_store_message) == 0) {
                printf("Piekarz: Otrzymałem komunikat o zamknięciu sklepu, kończę pracę.\n");
                break;
            }
        }
        for (int i = 0; i < MAX_PRODUKTOW; i++) {
            sem_wait(sem_id, i);  // Czekamy na dostęp do podajnika

            // Losowanie liczby wypieczonych produktów
            int ilosc_wypiekow = rand() % 5 + 1;
            printf("Piekarz: Wypiekłem %d sztuk produktu %s, próbuję dodać do podajnika.\n", ilosc_wypiekow, sklep->podajniki[i].produkt.nazwa);

            // Próba dodania produktów do podajnika
            if (sklep->podajniki[i].produkt.ilosc < MAX_PRODUKTOW_W_PODAJNIKU) {
                int wolne_miejsce = MAX_PRODUKTOW_W_PODAJNIKU - sklep->podajniki[i].produkt.ilosc;
                int do_dodania = (ilosc_wypiekow <= wolne_miejsce) ? ilosc_wypiekow : wolne_miejsce;

                sklep->podajniki[i].produkt.ilosc += do_dodania;
                sklep->statystyki_piekarza.wyprodukowane[i] += do_dodania;
                printf("Piekarz: Dodałem %d sztuk %s do podajnika.\n", do_dodania, sklep->podajniki[i].produkt.nazwa);

                // Jeśli zostały jeszcze produkty do dodania, wypisz komunikat
                if (ilosc_wypiekow > do_dodania) {
                    printf("Piekarz: Nie dodałem %d sztuk %s do podajnika, bo jest pełny.\n", ilosc_wypiekow - do_dodania, sklep->podajniki[i].produkt.nazwa);
                }
            } else {
                // Jeśli podajnik jest pełny
                printf("Piekarz: Podajnik %s pełny, nie mogę dodać produktów.\n", sklep->podajniki[i].produkt.nazwa);
            }

            sem_post(sem_id, i);  // Zwalniamy semafor
        }

        sleep(5);  // Piekarz czeka 10 sekund przed kolejnym wypiekiem

    }
}

int main (){
    setup_signal_handlers(cleanup_handler, evacuation_handler);

    int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    if (sklep == (Sklep *)-1) {
        perror("shmat");
        exit(1);
    }

    int sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    wypiekaj_produkty(sklep, sem_id);
    shmdt(sklep);
    return 0;
}