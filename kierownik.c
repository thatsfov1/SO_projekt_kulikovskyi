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
    
    if(signum == SIGUSR1) {
        printf("Kierownik: Stan kosza po zamknięciu sklepu:\n");
            for (int i = 0; i < MAX_PRODUKTOW; i++) {
                if (kosz->produkty[i].ilosc > 0) {
                    printf("%s %d szt.\n", kosz->produkty[i].nazwa, kosz->produkty[i].ilosc);
                }
        }
    }
    

    shmdt(sklep);
    shmdt(kosz);
    key_t key = ftok("/tmp", msq_kierownik);
        int msqid = msgget(key, 0666);
        if (msqid != -1) {
            msgctl(msqid, IPC_RMID, NULL);
    }

    printf(GREEN "Kierownik: Wszyscy klienci opuścili sklep. \n" RESET);
    if(signum == SIGUSR1) {
        printf(GREEN "Kierownik: Ewakuacja zakończona. \n" RESET);
    }
    exit(0);
}

void send_close_message() {
    key_t key = ftok("/tmp", msq_kierownik);
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }

    for (int i = 0; i < 10; i++) {
        message_buf sbuf;
        sbuf.mtype = 1;
        strcpy(sbuf.mtext, close_store_message);
        if (msgsnd(msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
    }
    
    printf(BLUE "Kierownik: Wkrótce sklep zamyka się, wszyscy klienci stojący w kolejce do kas będą obsłużeni, innych poproszę odłożyć towary do podajników i wyjść ze sklepu\n" RESET);
}

void evacuation_handler(int signum) {
    printf("Kierownik: Rozpoczynam ewakuację sklepu...\n");

    kill(0, SIGUSR1);
    
    while (sklep->ilosc_klientow > 0) {
        sleep(1);
    }

    cleanup_handler(SIGUSR1);
}

int main() {
    setup_signal_handlers(cleanup_handler, evacuation_handler);
    srand(time(NULL));

    // Losowanie czy będzie inwentaryzacja
    int czy_bedzie_inwentaryzacja = rand() % 5 + 1;
    if (czy_bedzie_inwentaryzacja==1) {
        printf(BLUE "Kierownik: Inwentaryzacja będzie przeprowadzona.\n" RESET);
        sklep->inwentaryzacja = 1;
    } else {
        printf(BLUE "Kierownik: Inwentaryzacja nie będzie przeprowadzona.\n" RESET);
    }
    
    // Losowanie czy będzie ewakuacja
    int czy_bedzie_ewakuacja = rand() % 10+1;
    if (czy_bedzie_ewakuacja==1) {
        sleep(rand() % CZAS_PRACY + 5);
        if (kill(0, SIGUSR1) == 0) {
                printf("Sygnał o ewakuację wysłany\n");
            } else {
                perror("Błąd wysyłania sygnału o ewakuację");
            }
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

    sleep(CZAS_PRACY);
    send_close_message();

    return 0;
}
