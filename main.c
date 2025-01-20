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
    printf(RED "\nZamykanie sklepu...\n" RESET);

    // Wysłanie sygnału do wszystkich procesów w grupie
    kill(0, SIGTERM);
 
    sleep(1);
    // Czyszczenie zasobów
    shmdt(sklep);
    shmdt(kosz);

    shmctl(shm_id, IPC_RMID, NULL);
    shmctl(kosz_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    // Usunięcie kolejki komunikatów kierownika
    key_t key = ftok("/tmp", msq_main);
    int msqid = msgget(key, 0666);
    if (msqid != -1) {
        msgctl(msqid, IPC_RMID, NULL);
    }

    // Usunięcie kolejek komunikatów kasjerów
    for (int i = 0; i < MAX_KASJEROW; i++) {
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
    printf("Main: Otrzymałem sygnał ewakuacji, kończę pracę.\n");
    cleanup(signum);
}

int main() {
    setup_signal_handlers(cleanup, evacuation_handler);

    // Tworzenie pamięci współdzielonej dla sklepu
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

    // Tworzenie pamięci współdzielonej dla kosza
    kosz_id = shmget(KOSZ_KEY, sizeof(Kosz), IPC_CREAT | 0666);
    if (kosz_id < 0) {
        perror("shmget kosz");
        exit(1);
    }

    kosz = (Kosz *)shmat(kosz_id, NULL, 0);
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

    // Inicjalizacja struktury sklepu
    sklep->ilosc_klientow = 0;
    sklep->inwentaryzacja = 0;
    init_produkty(sklep);
    init_kosz(kosz, sklep);
    sklep->sklep_zamkniety = 0;
    printf("Nacisnij 'e' dla wysłania sygnału o ewakuację.\n");

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

    // Tworzenie kolejki komunikatów dla kierownika
    // key_t kierownik_key = ftok("/tmp", msq_kierownik);
    // int kierownik_msqid = msgget(kierownik_key, 0666 | IPC_CREAT);
    // if (kierownik_msqid == -1) {
    //     perror("msgget kierownik");
    //     exit(1);
    // }

    key_t main_key = ftok("/tmp", msq_main);
    int main_msqid = msgget(main_key, 0666 | IPC_CREAT);
    if (main_msqid == -1) {
        perror("msgget main");
        exit(1);
    }
    message_buf rbuf;
    // Oczekiwanie na komunikat o zamknięciu sklepu
    
    // if (msgrcv(kierownik_msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1) {
    //     if (strcmp(rbuf.mtext, close_store_message) == 0) {
    //         printf(YELLOW "Main: Otrzymałem komunikat o zamknięciu sklepu, czekam na zakończenie procesów.\n" RESET);

    //         // Czekanie na zakończenie wszystkich procesów potomnych
    //         while (1) {
    //             if (wait(NULL) < 0 && errno == ECHILD) {
    //                 break;
    //             }
    //         }

    //         printf(GREEN "Main: Wszystkie procesy zakończone, kończę pracę.\n" RESET);
    //         cleanup(0);
    //     }
    // }

    if (msgrcv(main_msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1) {
        if (strcmp(rbuf.mtext, ready_to_close) == 0) {
            printf(GREEN "Main: Wszystkie procesy gotowe do zamknięcia, kończę pracę.\n" RESET);
            cleanup(0);
        }
    }

    // Główna pętla oczekiwania na zakończenie procesów
    while (1) {
        if (wait(NULL) < 0 && errno == ECHILD) {
            break;
        }
    }

    cleanup(0);

    return 0;
}