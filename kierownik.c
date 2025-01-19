#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/msg.h>
#include "struktury.h"
#include "funkcje.h"

Sklep *sklep;
Kosz *kosz;
int shm_id;
int kosz_id;

void cleanup_handler(int signum) {
    if (sklep->inwentaryzacja) {
        printf("Kierownik: Na podajnikach zostało: ");
        for (int i = 0; i < MAX_PRODUKTOW; i++) {
            if (sklep->podajniki[i].produkt.ilosc > 0) {
                printf("%s %d szt. ", sklep->podajniki[i].produkt.nazwa, sklep->podajniki[i].produkt.ilosc);
            }
        }
        printf("\n");
    }
    shmdt(sklep);
    shmdt(kosz);
    exit(0);
}

void evacuation_handler(int signum) {
    printf("Kierownik: Rozpoczynam ewakuację sklepu...\n");

    // Informing all processes to stop their operations
    kill(0, SIGUSR1);
    
    // Wait for all customers to exit
    while (sklep->ilosc_klientow > 0) {
        sleep(1);
    }

    for (int i = 0; i < MAX_KASJEROW; i++) {
        key_t key = ftok("/tmp", i + 1);
        int msqid = msgget(key, 0666);
        if (msqid != -1) {
            msgctl(msqid, IPC_RMID, NULL);
        }
    }

    printf("Kierownik: Wszyscy klienci opuścili sklep. Zamykanie sklepu...\n");
    cleanup_handler(signum);
}

int main() {
    signal(SIGTERM, cleanup_handler);
    signal(SIGUSR1, evacuation_handler);

    srand(time(NULL));

    int will_inwentaryzacja = rand() % 2;
    if (will_inwentaryzacja) {
        printf("Kierownik: Inwentaryzacja będzie przeprowadzona.\n");
        sklep->inwentaryzacja = 1;
    } else {
        printf("Kierownik: Inwentaryzacja nie będzie przeprowadzona.\n");
    }

    shm_id = shmget(SHM_KEY, sizeof(Sklep), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }
    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    if (sklep == (Sklep *)-1) {
        perror("shmat");
        exit(1);
    }

    kosz_id = shmget(KOSZ_KEY, sizeof(Kosz), 0666);
    if (kosz_id < 0) {
        perror("shmget kosz");
        exit(1);
    }
    kosz = (Kosz *)shmat(kosz_id, NULL, 0);
    if (kosz == (Kosz *)-1) {
        perror("shmat kosz");
        exit(1);
    }

    char ch;
    while ((ch = getchar()) != EOF) {
        if (ch == 'e') {
            if (kill(getpid(), SIGUSR1) == 0) {
                printf("Sygnał o ewakuację wysłany\n");
            } else {
                perror("Błąd wysyłania sygnału o ewakuację");
            }
        }
    }

    return 0;
}
