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
int sem_id;
int kosz_id;
int msqid;

// Inicjalizacja kolejki komunikatów do komunikacji z procesami
void initialize_message_queue() {
    key_t key = ftok("/tmp", msq_kierownik);
    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }
}

// Czyszczenie kolejki komunikatów
void cleanup_message_queue() {
    if (msqid != -1) {
        msgctl(msqid, IPC_RMID, NULL);
    }
}

// Wydrukowanie stanu inwentaryzacji
void print_inventory() {
    printf("Kierownik: Na podajnikach zostało: ");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        if (sklep->podajniki[i].produkt.ilosc > 0) {
            printf("%s %d szt. ", sklep->podajniki[i].produkt.nazwa, sklep->podajniki[i].produkt.ilosc);
        }
    }
    printf("\n");

    key_t piekarz_key = ftok("/tmp", msq_piekarz);
    int piekarz_msqid = msgget(piekarz_key, 0666);
    if (piekarz_msqid != -1) {
        message_buf rbuf;
        for (int i = 0; i < MAX_PRODUKTOW; i++) {
            if (msgrcv(piekarz_msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1) {
                printf("Piekarz: Wyprodukował %s %s szt.\n", sklep->podajniki[i].produkt.nazwa, rbuf.mtext);
            }
        }
    }

    for (int i = 0; i < MAX_KASJEROW; i++) {
        key_t kasjer_key = ftok("/tmp", i + 1);
        int kasjer_msqid = msgget(kasjer_key, 0666);
        if (kasjer_msqid != -1) {
            message_buf rbuf;
            for (int j = 0; j < MAX_PRODUKTOW; j++) {
                if (msgrcv(kasjer_msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1) {
                    printf("Kasjer %d: Sprzedał %s %s szt.\n", i + 1, sklep->podajniki[j].produkt.nazwa, rbuf.mtext);
                }
            }
        }
    }
}

// Wydrukowanie stanu kosza po zamknięciu sklepu
void print_kosz() {
    printf("Kierownik: Stan kosza po zamknięciu sklepu:\n");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        if (kosz->produkty[i].ilosc > 0) {
            printf("%s %d szt.\n", kosz->produkty[i].nazwa, kosz->produkty[i].ilosc);
        }
    }
}

// Wysłanie komunikatu o zamknięciu sklepu do wszystkich procesów
void send_close_message() {
    key_t keys[5] = {
        ftok("/tmp", 1), // Kasa 1
        ftok("/tmp", 2), // Kasa 2
        ftok("/tmp", 3), // Kasa 3
        ftok("/tmp", msq_piekarz), // Piekarz
        ftok("/tmp", msq_klient) // Klient
    };
    for (int i = 0; i < 5; i++) {
        int msqid = msgget(keys[i], 0666 | IPC_CREAT);
        if (msqid == -1) {
            perror("msgget");
            exit(1);
        }

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

// Wysłanie komunikatu do main.c o gotowości do zamknięcia
void send_close_message_to_main() {
    key_t main_key = ftok("/tmp", msq_main);
    int main_msqid = msgget(main_key, 0666 | IPC_CREAT);
    if (main_msqid == -1) {
        perror("msgget main");
        exit(1);
    }

    message_buf sbuf;
    sbuf.mtype = 1;
    strcpy(sbuf.mtext, ready_to_close);
    if (msgsnd(main_msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1) {
        perror("msgsnd main");
        exit(1);
    }
}

// Oczekiwanie na potwierdzenia od wszystkich procesów
void wait_for_acknowledgments() {
    message_buf rbuf;
    int ack_count = 0;
    while (ack_count < 5) {
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1) {
            if (strcmp(rbuf.mtext, acknowledgment_to_kierownik) == 0) {
                ack_count++;
                printf(RED "Kierownik: Otrzymano potwierdzenie %d/5\n" RESET, ack_count);
            }
        }
    }

    send_close_message_to_main();
}

// Funkcja czyszcząca, która odłącza pamięć współdzieloną i wysyła potwierdzenia
void cleanup_handler(int signum) {

    if (sklep->inwentaryzacja) {
        print_inventory();
    }

    if (signum) {
        print_kosz();
    }

    shmdt(sklep);
    shmdt(kosz);
    

    printf(GREEN "Kierownik: Wszyscy klienci opuścili sklep. \n" RESET);
    if (signum) {
        printf(GREEN "Kierownik: Ewakuacja zakończona. \n" RESET);
    }

    cleanup_message_queue();
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum) {
    printf("Kierownik: Rozpoczynam ewakuację sklepu...\n");

    while (sklep->ilosc_klientow > 0) {
        sleep(1);
    }
    //send_close_message_to_main();

    cleanup_handler(SIGUSR1);
}

int main() {
    setup_signal_handlers(cleanup_handler, evacuation_handler);
    srand(time(NULL));

    initialize_message_queue();

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
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

    // int czy_bedzie_inwentaryzacja = 1;
    // if (czy_bedzie_inwentaryzacja == 1) {
    //     printf(BLUE "Kierownik: Inwentaryzacja będzie przeprowadzona.\n" RESET);
    //     sklep->inwentaryzacja = 1;
    // } else {
    //     printf(BLUE "Kierownik: Inwentaryzacja nie będzie przeprowadzona.\n" RESET);
    // }

    int czy_bedzie_ewakuacja = 1;
    if (czy_bedzie_ewakuacja == 1) {
        sleep(rand() % CZAS_PRACY + 5);
        if (kill(0, SIGUSR1) == 0) {
            printf("Sygnał o ewakuację wysłany\n");
        } else {
            perror("Błąd wysyłania sygnału o ewakuację");
        }
    }

    sleep(CZAS_PRACY);
    send_close_message();

    
    wait_for_acknowledgments();
    cleanup_handler(0);
    return 0;
}