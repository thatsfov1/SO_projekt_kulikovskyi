#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include "struktury.h"
#include "funkcje.h"

int shm_id;
int sem_id;
int kosz_id;
Sklep *sklep;
Kosz *kosz;

void cleanup(int signum) {
    printf("\nZamykanie sklepu...\n");

    // Wysłanie sygnału do wszystkich procesów w grupie
    kill(0, SIGTERM);
 
    // Czyszczenie zasobów
    shmdt(sklep);
    shmdt(kosz);

    shmctl(shm_id, IPC_RMID, NULL);
    shmctl(kosz_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    exit(0);
}

void evacuation_handler(int signum) {
    printf("Main: Otrzymałem sygnał ewakuacji, kończę pracę.\n");
    cleanup(signum);
}

void init_produkty(Sklep *sklep) {
    char *nazwy[MAX_PRODUKTOW] = {
        "Chleb", "Bułka", "Rogalik", "Bagietka", "Pączek",
        "Ciasto", "Tort", "Muffin", "Keks", "Babka",
        "Sernik", "Makowiec"
    };
    float ceny[MAX_PRODUKTOW] = {
        3.0, 1.0, 2.5, 4.0, 2.0,
        10.0, 50.0, 3.5, 15.0, 20.0,
        25.0, 30.0
    };

    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        strcpy(sklep->podajniki[i].produkt.nazwa, nazwy[i]);
        sklep->podajniki[i].produkt.cena = ceny[i];
        sklep->podajniki[i].produkt.ilosc = 0;
        sklep->podajniki[i].produkt.id = i;
    }
}

void init_kosz(Kosz *kosz, Sklep *sklep) {
    memset(kosz, 0, sizeof(Kosz));
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        kosz->produkty[i].id = i;
        strcpy(kosz->produkty[i].nazwa, sklep->podajniki[i].produkt.nazwa); // Copy product names from Sklep
        kosz->produkty[i].ilosc = 0;
        kosz->produkty[i].cena = sklep->podajniki[i].produkt.cena;
    }
}

int main() {
    // Rejestracja funkcji cleanup do obsługi sygnałów
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGUSR1, evacuation_handler);

    // Tworzenie pamięci współdzielonej
    shm_id = shmget(SHM_KEY, sizeof(Sklep), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    if (sklep == (Sklep *)-1) {
        perror("shmat");
        exit(1);
    }

    kosz_id = shmget(KOSZ_KEY, sizeof(Kosz), IPC_CREAT | 0666);
    if (kosz_id < 0) {
        perror("shmget kosz");
        exit(1);
    }

    Kosz *kosz = (Kosz *)shmat(kosz_id, NULL, 0);
    if (kosz == (Kosz *)-1) {
        perror("shmat kosz");
        exit(1);
    }

    // Tworzenie semaforów
    sem_id = semget(SEM_KEY, 16, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    // Inicjalizacja semaforów
    for (int i = 0; i < 16; i++) {
        semctl(sem_id, i, SETVAL, 1);
    }

    sklep->ilosc_klientow = 0;
    sklep->inwentaryzacja = 0;
    init_produkty(sklep);
    init_kosz(kosz, sklep);
    printf("Nacisnij 'e' dla wysłania sygnału o ewakuację.\n");

    // Uruchamianie procesów
    if (fork() == 0) {
        execl("./kierownik", "./kierownik", NULL);
    }

    if ((fork() == 0)) {
        execl("./piekarz", "./piekarz", NULL);
    }

    if ((fork() == 0)) {
        execl("./kasjer", "./kasjer", NULL);
    }

    if ((fork() == 0)) {
        execl("./klient", "./klient", NULL);
    }

    sleep(CZAS_PRACY);
    //  Wysłanie sygnału zamknięcia sklepu
    printf("Sklep zamyka się, wszyscy klienci w kolejce będą obsłużeni\n");
    
    kill(0, SIGTERM);
    while(1) {
        if (wait(NULL) < 0 && errno == ECHILD) {
            break;
        }
    }

    cleanup(0);

    return 0;
}