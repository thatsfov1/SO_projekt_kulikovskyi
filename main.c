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

// Funkcja czyszcząca, która odłącza pamięć współdzieloną i usuwa semafory oraz kolejki komunikatów
void cleanup(int signum) {

    while(wait(NULL) > 0);
    //sleep(1);
    printf(RED "\nZamykanie sklepu...\n" RESET);

    // Czyszczenie zasobów
    shmdt(sklep);

    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    // Usunięcie kolejek komunikatów kasjerów
    for (int i = 0; i < LICZBA_KOLEJEK; i++) {
        key_t key = ftok("/tmp", i + 1);
        int msqid = msgget(key, 0666);
        if (msqid != -1) {
            msgctl(msqid, IPC_RMID, NULL);
        }
    }
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum) {
    cleanup(signum);
}

int main() {
    setup_signal_handlers(cleanup, evacuation_handler);
    signal(SIGUSR2, cleanup);
    // Tworzenie pamięci współdzielonej dla sklepu

    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    // Tworzenie semaforów
    init_semaphores(&sem_id, SEM_KEY, 21);

    // Inicjalizacja semaforów
    init_semaphore_values(sem_id, 21);

    // Inicjalizacja struktury sklepu
    sklep->ilosc_klientow = 0;
    sklep->inwentaryzacja=0;
    init_produkty(sklep);
    sklep->sklep_zamkniety = 0;

    // Uruchamianie procesów
    pid_t kierownik_pid, piekarz_pid, kasjer_pid, klient_pid;

    if ((kierownik_pid = fork()) == 0) {
        execl("./kierownik", "./kierownik", NULL);
    }

    if ((piekarz_pid = fork()) == 0) {
        execl("./piekarz", "./piekarz", NULL);
    }

    if ((kasjer_pid = fork()) == 0) {
        execl("./kasjer", "./kasjer", NULL);
    }

    if ((klient_pid = fork()) == 0) {
        execl("./klient", "./klient", NULL);
    }

    cleanup(0);

    return 0;
}